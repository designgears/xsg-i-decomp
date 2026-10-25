/*
 * OV01 original TU 3: 0x00a07770..0x00a10bb8 (150 functions)
 */
#include "common.h"
#include "shared.h"
#include "battle_init.h"
#include "ov01/calc.h"
#include "main/party.h"

#define CURSOR_STATE_OFF 0x10
#define STATE_FLAGS_OFF 0x00
#define STATE_UNK70_OFF 0x70
#define STATE_UNK74_OFF 0x74

/*
 * Two CalcUnitParam (include/ov01/calc.h, defined in ov01/tu004) bytes
 * battleCtrlBoostPl/battleCtrlBoostEn read as signed: the enemy AI's own
 * boost chance percentage (lb at +0x20, inside calc.h's unmodeled_1e span)
 * and the boost-level value both functions log (lb at +0x3C, inside
 * calc.h's unmodeled_3a span). calc.h is owned by a different TU and is not
 * completed here.
 */
#define CALC_PARAM_BOOST_CHANCE_OFF 0x20
#define CALC_PARAM_BOOST_LEVEL_OFF 0x3C

extern int escapeFlag;
extern const char D_00A44080[];
extern int printf(const char *format, ...);

const char **dataItmNameGet(int index);
const char **dataWepNameGet(int index);
const char **dataBltNameGet(int index);
const char **dataAccNameGet(int index);

extern void battleMsgPut(int x, int y, int msgId, int frames);

/*
 * The battle actor object a unit's actor/weapon slots point to. src/main/
 * chr.h, src/main/near_dir.h, src/main/set_motion.h and src/ov01/unit_cmd.h
 * all restate this same 0x70-byte head verbatim (a TU-local header cannot be
 * included from another TU); this TU adds only the float its own code
 * touches, at +0xC0 (transWepIn/transWepOut set it to 0.0/1.0, the same
 * "transparency" member src/ov01/unit_cmd.h restates at that offset).
 */
typedef struct Actor {
    u32 flags;
    void (*update)(struct Actor *actor);
    void (*draw)(struct Actor *actor);
    u32 quadword_alignment_gap;
    Vector4 position;
    Vector4 previous_position;
    Vector4 velocity;
    Vector4 acceleration;
    Vector4 rotation;
    Vector4 scale;
    unsigned char unmodeled_70[0xC0 - 0x70];
    float transparency; /* +0xC0 */
} Actor;

#define WEAPON_HIDDEN_BIT 0x8

/*
 * The unit record scenarioMtd, transWepIn/transWepOut and spWepDispOn
 * (0x00a0e550, still INCLUDE_ASM here) all share: spWepDispOn receives the
 * same pointer scenarioMtd receives, dereferences it to reach this record,
 * then indexes its +0x1C table by (weapon-slot & 3) and reads +0x14, exactly
 * as transWepIn/transWepOut do on their own argument and scenarioMtd does on
 * *unitHandle. Bytes between the evidenced members are untouched here.
 */
typedef struct BattleUnit BattleUnit;
struct BattleUnit {
    unsigned char unmodeled_00[0x10];
    Actor *work;       /* +0x10 */
    Actor *actor;      /* +0x14 */
    unsigned char unmodeled_18[0x1C - 0x18];
    Actor *weapon[4];  /* +0x1C */
};

/*
 * BattleUnit's own ObjectTask.work slot at +0x10 (inside unmodeled_00 above,
 * not completed here): camEventTurnStartSet reads it as the same Actor
 * pointer src/ov01/unit_cmd.c and src/ov01/snd.c already read through
 * ObjectTask.work on their own unit records, testing bit 0x40 of
 * Actor.flags (the enemy bit src/ov01/unit_cmd.h names ACTOR_FLAG_ENEMY).
 */
#define BATTLE_UNIT_WORK_OFF 0x10

/*
 * The camera-event request record camEventExec (defined elsewhere, still
 * INCLUDE_ASM there) consumes: camEventTurnStartSet writes type 6, the
 * acting/target units and, in param, whether the acting unit is an enemy.
 * battleCtrlMan writes type 8 with only unit/target, leaving param at
 * whatever the record already held. Data still asm-owned; the original ELF
 * local symbol is "camEv", not yet in config/symbols/ov01.txt, so the splat
 * placeholder name is kept here.
 */
typedef struct CamEvent {
    int type;           /* +0x00 */
    int param;           /* +0x04 */
    BattleUnit *unit;     /* +0x08 */
    BattleUnit *target;   /* +0x0C */
} CamEvent;
extern CamEvent D_00A57B80;

extern void camEventExec(CamEvent *event, BattleUnit *unit);

/*
 * The scenario-battle exec record the scenarioBatExecPhase* handlers work on
 * (scenarioBatExecPhase20 passes the same pointer to etehrCVGet and to
 * scenarioMtd, and every unit call there loads its first word). Only +0x00,
 * the acting unit, is read by this TU's C; spWepDispOn (still INCLUDE_ASM)
 * also reads +0x04, +0x08 and +0x0C, which are not modeled here.
 */
typedef struct ScenarioBatExec {
    BattleUnit *unit; /* +0x00 */
} ScenarioBatExec;

int dataMtdRead(BattleUnit *unit, int trigger);
int dataDefWpnGet(BattleUnit *unit);

/*
 * Show (transWepIn) or hide (transWepOut) the weapon actor in the unit's
 * slot `slot & 3` by installing its fade draw callback.
 */
void transWepIn(BattleUnit *unit, int slot);
void transWepOut(BattleUnit *unit, int slot);

void spWepDispOn(ScenarioBatExec *exec, int weapon);
void spWepDispOff(ScenarioBatExec *exec, int weapon);
void hairWindOn(BattleUnit *unit);
void hairWindOff(BattleUnit *unit);
extern void ACT_setHand(Actor *actor, int hand);

extern void transWepInDraw(Actor *actor);
extern void transWepOutDraw(Actor *actor);

/*
 * The battle AI script identifier (data still asm-owned: the extern keeps
 * its splat name until the owning data TU recovers it, docs/naming.md).
 */
extern int D_00A57C80;

/*
 * The unit currently executing a battle action / the unit that most
 * recently acted (data still asm-owned; original ELF symbol names).
 */
extern BattleUnit *pActUnit;
extern BattleUnit *pLastUnit;
BattleUnit **tgtUnitAdrGet(void);
BattleUnit *tgtUnitGet(void);

/*
 * manWorkGet's return type. Only the two fields menuKeyGuideObjDraw (src/
 * ov01/menu.c, ov01/tu009) consumes are evidenced there: unitTask at +0x10
 * and controlPhase at +0xc. This TU's own code only takes the record's
 * address (data still asm-owned; original ELF symbol name).
 */
typedef struct ManWork {
    unsigned char unmodeled_00[0xc];
    int controlPhase;     /* +0xc */
    ObjectTask *unitTask; /* +0x10 */
} ManWork;
extern ManWork manWk;

/*
 * Opaque battle-control record. batCtrlGet is its only reference in the
 * mapped functions so far; no dereference evidences any member layout
 * (data still asm-owned; original ELF symbol name).
 */
typedef struct BatCtrl BatCtrl;
extern BatCtrl batCtrl;

/*
 * Scenario-battle phase-dispatch record. scenarioBatInit and every
 * scenarioBatExecPhase* handler (still INCLUDE_ASM here) call
 * scenarioPtrFuncSet to install the next phase's handler and reset its
 * frame counter; only the two words that function writes are evidenced:
 *   +0x10 nextPhase: the phase callback the caller passes in $a0;
 *   +0x14 phaseTimer: zeroed on every phase transition.
 * scenarioBatInit itself also writes +0x00, +0x04, +0x08 and +0x0c, which
 * are not modeled here (data still asm-owned; original ELF symbol name).
 */
/*
 * scenarioBatNext (0x00a0ba30) evidences three more of this record's
 * words: it adds source->stride into index, clamps index to count once
 * their sum reaches it, and reports whether the clamp fired. +0x08 stays
 * unmodeled (scenarioBatInit writes it, but no claimed function here reads
 * it).
 */
/*
 * Opaque object ScenarioBatState.source points to; scenarioBatNext is the
 * only reference to it in this TU and touches just the one evidenced word.
 */
typedef struct ScenarioStep {
    unsigned char unmodeled_00[0x10];
    int stride; /* +0x10: added into ScenarioBatState.index by scenarioBatNext */
} ScenarioStep;
typedef void (*ScenarioPhaseFunc)(void);
/*
 * scenarioBatInit also writes +0x08, the destination buffer scenarioBatInit
 * pairs with source: both are set from the fixed scenarioSrc/scenarioDst
 * byte arrays, so dest is typed the same as source's own backing array.
 */
typedef struct ScenarioBatState {
    int count;                    /* +0x00: index is clamped to this */
    int index;                    /* +0x04: advanced by source->stride each call */
    unsigned char *dest;          /* +0x08 */
    ScenarioStep *source;         /* +0x0C */
    ScenarioPhaseFunc nextPhase; /* +0x10 */
    int phaseTimer;              /* +0x14 */
} ScenarioBatState;
extern ScenarioBatState scenarioPtr;

extern BattleUnit *pExecUnit;
extern BattleUnit *pManUnit;
extern int thinkWk;
extern int helpFlag;
extern int menuFlag;
extern int batMainFlag;
extern int fifoCmd;
extern int pCounterUnit;
extern BattleUnit *pTgtUnit2;
extern BattleUnit *pTgtUnit;
extern ObjectTask *pTaskEnCur;
ObjectTask *objEnCurCreate(float x, float y, float z);
void keyBuffClear(void);
void actStockInit(void);
void scenarioBatInit(void);
void monsGainInit(void);
struct BatCtrlL { int unk0; int unk4; int unk8; };
struct ManWkL { int unk0; int unk4; int unk8; int controlPhase; ObjectTask *unitTask; };
#define MW ((struct ManWkL *)&manWk)
#define BC ((struct BatCtrlL *)&batCtrl)

void battleInit(void)
{
  ObjectTask *task;
  objInit();
  unitInit();
  thinkSysInit();
  dataFileInfoInit();
  task = objEnCurCreate(0.0f, 0.0f, 0.0f);
  *((int *) task->work) |= 4;
  pTaskEnCur = task;
  pActUnit = 0;
  pExecUnit = 0;
  pTgtUnit = 0;
  pTgtUnit2 = 0;
  pManUnit = 0;
  pCounterUnit = 0;
  pLastUnit = 0;
  fifoInit(&fifoCmd, 0x20);
  keyBuffClear();
  ((struct ManWkL *) (&manWk))->unitTask = 0;
  thinkWk = 0;
  ((struct BatCtrlL *) (&batCtrl))->unk4 = 7;
  batMainFlag = 0;
  ((struct ManWkL *) (&manWk))->controlPhase = (int) 0;
  menuFlag = 0;
  helpFlag = 0;
  escapeFlag = 0;
  ((struct ManWkL *) (&manWk))->unk4 = 0;
  ((struct ManWkL *) (&manWk))->unk0 = 0;
  ((struct BatCtrlL *) (&batCtrl))->unk0 = 1;
  ((struct BatCtrlL *) (&batCtrl))->unk8 = 1;
  dataBattleInit();
  actStockInit();
  if (!task->work)
  {
  }
  do
  {
    scenarioBatInit();
    monsGainInit();
    sndInit();
    MBattleCreate();
    menuInitBat();
  }
  while (0);
  dataVPadModeSet(0);
}

void battleCtrl(void);
void battleExec(void);
int battleEndChk(void);

/*
 * Set unconditionally at the start of every battleMain tick (data still
 * asm-owned; original ELF symbol name); no other write is evidenced in this TU.
 */
extern int batMainFlag;

void battleMain(void)
{
    /* Separate load keeps this store in its own register. */
    int flag = 1;
    batMainFlag = flag;
    battleCtrl();
    battleExec();
    battleEndChk();
}

extern const char D_00A43CB8[];
extern const char D_00A43CC8[];
extern const char D_00A43CD8[];
extern void unitCmdSet(ObjectTask *unit, int *cmd);
extern int dataPadRead(int padNo);
extern void menuStatDispRemove(void);
extern void objCurRemove(void);
extern int busyUnitChk(int side);
extern int battleRetCodeGet(void);

int battleAfter(void)
{
  int rv;
  int cmd[8];
  CamEvent *new_var;
  BattleUnit **unitTable;
  int new_var2;
  int count;
  int i;
  int ret;
  BattleUnit *unit;
  ret = battleRetCodeGet();
  if ((new_var && new_var) && new_var)
  {
  }
  if (ret == 1)
  {
    if (batMainFlag == ret)
    {
      char *msg;
      i = 0;
      printf(D_00A43CB8);
      batMainFlag = 2;
      (&D_00A57B80)->unit = pLastUnit;
      new_var2 = 5;
      do
      {
        new_var = &D_00A57B80;
        D_00A57B80.type = new_var2;
        batMainFlag = 2;
        camEventExec(new_var, pLastUnit);
        count = unitTblGet(0, &unitTable);
      }
      while (0);
      if (count > 0)
      {
        do
        {
          unit = unitTable[i];
          i++;
          if ((unit != 0) && (!((*((int *) unit->work)) & 0x80)))
          {
            cmd[0] = 0x26;
            unitCmdSet((ObjectTask *) unit, cmd);
          }
        }
        while (i < count);
      }
      if ((D_00A43CD8 && D_00A43CD8) && D_00A43CD8)
      {
      }
      menuStatDispRemove();
      objCurRemove();
      return 1;
    }
    if (dataPadRead(1) & 0x20)
    {
      goto zero;
    }
    if (busyUnitChk(2) != 0)
    {
      goto one;
    }
    goto zero;
    one:
    goto _sw_tl2;

    zero:
    return 0;

  }
  if (ret == 3)
  {
    printf(D_00A43CC8);
    batMainFlag = 2;
    menuStatDispRemove();
    objCurRemove();
    return 0;
  }
  printf(D_00A43CD8);
  batMainFlag = (double) 2;
  menuStatDispRemove();
  objCurRemove();
  new_var = &D_00A57B80;
  return 0;
  _sw_tl2:
  return 1;

}

extern int dataPadRead(int padNo);
int battleCtrlBoostPl(void);
int battleCtrlConfusion(BattleUnit *unit);
int battleCtrlMan(BattleUnit *unit, int *inp);
int battleCtrlThink(ObjectTask *unit);
int battleCtrlJunk(ObjectTask *unit);
int unitLiveNumGet2(int kind);
int actUnitTblMake(int side, BattleUnit **actUnits);
int calcStatGet(BattleUnit *unit, int stat, int mask);
int rnd(int max);
ManWork *manWorkGet(void);
int battleCtrlHelp(void);
int battleCtrlBoostEn(void);
ObjectTask *actStockPick(void);
void menuStockNext(void);
void menuStockStart(void);
void menuStockBoost(void);
void calcTurnStart(BattleUnit *unit);
void thinkTurnStartExec(BattleUnit *unit);
void camEventTurnStartSet(void);
int thinkExecChk(void);
int battleMsgEndChk(void);
extern int D_00A57C7C;

void battleCtrl(void)
{
    struct BatCtrlL *bc;
    int cmd[8];
    int ret;
    int st;

    if (dataPadRead(1) & 5) {
        battleCtrlHelp();
    }
    if (battleCtrlBoostPl() != 0) {
        return;
    }
    switch (BC->unk0) {
    case 1:
        bc = BC;
        if (unitLiveNumGet2(0) == 0) {
            break;
        }
        if (unitLiveNumGet2(1) == 0) {
            break;
        }
        if (actUnitTblMake(2, (BattleUnit **)cmd) == 0) {
            bc->unk0 = 2;
            manWorkGet()->unitTask = 0;
            break;
        }
        if (calcStatGet(pActUnit, 1, 0x4000) != 0 || calcStatGet(pActUnit, 5, 0x1000) != 0) {
            ret = battleCtrlConfusion(pActUnit);
        } else if (calcStatGet(pActUnit, 4, 0x40) != 0 && rnd(100) < 50) {
            ret = battleCtrlJunk((ObjectTask *)pActUnit);
        } else {
            goto normal;
        }
        if (ret == 0) {
            bc->unk0 = 3;
        }
        break;
    normal:
        if (pActUnit->work->flags & 0x20) {
            ret = ((int (*)(BattleUnit *, int *))battleCtrlThink)(pActUnit, &thinkWk);
        } else {
            ret = battleCtrlMan(pActUnit, (int *)manWorkGet());
        }
        if (ret == 0) {
            BC->unk0 = 3;
        }
        break;
    case 2:
        {
            BattleUnit *pick = (BattleUnit *)actStockPick();
            pActUnit = pick;
            if (pick == 0) {
                break;
            }
        }
        if (D_00A57C7C == 0) {
            menuStockNext();
        } else if (D_00A57C7C == 1) {
            menuStockStart();
        } else {
            menuStockBoost();
        }
        pActUnit->work->flags |= 8;
        calcTurnStart(pActUnit);
        thinkTurnStartExec(pActUnit);
        camEventTurnStartSet();
        battleCtrlBoostEn();
        BC->unk0 = 1;
        break;
    case 3:
        if (pActUnit->work->flags & 8) {
            break;
        }
        st = BC->unk4;
        if (st != 1) {
            break;
        }
        if (thinkExecChk() == 0) {
            break;
        }
        if (battleMsgEndChk() == 0) {
            break;
        }
        BC->unk0 = st;
        D_00A57B80.type = 7;
        D_00A57B80.target = 0;
        D_00A57B80.param = (*(int *)((union { void *p; int i; } *)&((ObjectTask *)pActUnit)->work)->p >> 6) & 1;
        D_00A57B80.unit = pActUnit;
        ((void (*)(CamEvent *))camEventExec)(&D_00A57B80);
        break;
    }
}

typedef struct ConfTgt {
    int slot;
    int wpn;
    BattleUnit *tgt;
} ConfTgt;
extern ConfTgt D_00A57300[];
extern char *dataWpnGet(int no);
extern int unitTblGet(int side, BattleUnit ***table);
extern CalcUnitParam *calcUPGet(ObjectTask *unit);
extern int calcAtkChk(ObjectTask *unit, BattleUnit *tgt, int tid);
int unitCmdListSet(ObjectTask *unit, int *cmd);

int battleCtrlConfusion(BattleUnit *unit)
{
    int cmd[8];
    BattleUnit **units;
    int cnt;
    int count;
    int slot;
    int k;
    int wpn;
    int no;
    int sp24;
    int off2;
    int off;

    count = 0;
    cnt = unitTblGet(2, &units);
    if (*(unsigned short *)((char *)calcUPGet((ObjectTask *)unit) + 0x1C) & 0x40) {
        slot = 0;
        off = 0x50;
        do {
            no = *(short *)((char *)calcUPGet((ObjectTask *)unit) + off + 0xE);
            if (no != 0) {
                wpn = *(short *)((char *)dataWpnGet(no) + 0x14);
                for (k = 0; k < cnt; k++) {
                    if (units[k] != 0 && !(units[k]->work->flags & 0x80) && units[k] != unit && calcAtkChk((ObjectTask *)unit, units[k], wpn) != 0) {
                        D_00A57300[count].slot = slot;
                        D_00A57300[count].wpn = wpn;
                        D_00A57300[count].tgt = units[k];
                        count++;
                    }
                }
            }
            slot++;
            off += 2;
        } while (slot < 3);
    } else {
        if ((unit->work->flags & 0x40) || (*(short *)((char *)calcUPGet((ObjectTask *)unit) + 0x5E) == 0)) {
            sp24 = -1;
        } else {
            sp24 = 0;
        }
        slot = 0;
        off2 = 0x70;
        do {
            wpn = *(short *)((char *)calcUPGet((ObjectTask *)unit) + off2 + 6);
            if (wpn != 0) {
                for (k = 0; k < cnt; k++) {
                    if (units[k] != 0 && !(units[k]->work->flags & 0x80) && units[k] != unit && calcAtkChk((ObjectTask *)unit, units[k], wpn) != 0) {
                        D_00A57300[count].slot = sp24;
                        D_00A57300[count].wpn = wpn;
                        D_00A57300[count].tgt = units[k];
                        count++;
                    }
                }
            }
            slot++;
            off2 += 2;
        } while (slot < 6);
    }
    if (count > 0) {
        int r = rnd(count - 1);
        cmd[0] = 1;
        cmd[1] = D_00A57300[r].wpn;
        cmd[2] = (int)D_00A57300[r].tgt;
        cmd[3] = D_00A57300[r].slot;
        unitCmdListSet((ObjectTask *)unit, cmd);
    }
    cmd[0] = 8;
    unitCmdListSet((ObjectTask *)unit, cmd);
    return 0;
}

typedef struct MsgText {
    signed char text[0x23];
} MsgText;
extern MsgText msgWork;
extern const MsgText D_00A43CE8;
void battleMsg(int msgId);
int unitCmdListSet(ObjectTask *unit, int *cmd);

int battleCtrlJunk(ObjectTask *unit)
{
    int cmd[8];

    msgWork = D_00A43CE8;
    battleMsg((int)&msgWork);
    cmd[0] = 8;
    unitCmdListSet(unit, cmd);
    return 0;
}

extern int actStock;

int actStockMaxGet(void);
typedef struct {
    int a[4];
    int b[2];
    int max;
    int c;
    int d;
    int e;
} ActStockView;

void actStockInit(void)
{
    int i;

    ((ActStockView *)&actStock)->max = actStockMaxGet();
    ((ActStockView *)&actStock)->c = 0;
    ((ActStockView *)&actStock)->d = 0;
    ((ActStockView *)&actStock)->e = 0;
    for (i = 0; i < 4; i++)
        ((ActStockView *)&actStock)->a[i] = 0;
    for (i = 0; i < 2; i++)
        ((ActStockView *)&actStock)->b[i] = 0;
}

extern int unitTblGet(int side, BattleUnit ***table);
extern int calcBattleChk(ObjectTask *unit);

int actStockMaxGet(void)
{
  BattleUnit **table;
  int count;
  int i;
  int valid;
  int is_small;
  i = 0;
  valid = 0;
  count = unitTblGet(2, &table);
  if (count > 0)
  {
    do
    {
      if (table[i] != 0)
      {
        if (calcBattleChk((ObjectTask *) table[i]) != 0)
        {
          valid++;
        }
      }
      i++;
    }
    while (i < count);
  }
  is_small = valid < 5;
  if (valid > 0)
  {
    if (((valid * 5) % 10) >= 5)
    {
      valid = (valid / 2) + 1;
    }
    else
    {
      valid = valid / 2;
    }
    ;
  }
  return (valid < 5) ? (valid) : (4);
}

extern const char D_00A43D10[];
extern const char D_00A43D30[];
extern const char D_00A43D40[];
extern int calcWctDec(ObjectTask *unit);

void actStockMake(void)
{
    BattleUnit **table;
    int count;
    int n;
    int i;
    int *q;
    int *p;

    n = 0;
    i = 3;
    p = (int *)&actStock;
    ((ActStockView *)p)->max = actStockMaxGet();
    ((ActStockView *)p)->c = 0;
    count = unitTblGet(2, &table);
    p = (int *)&actStock;
    do {
        p[i] = 0;
        i--;
    } while (i >= 0);
    if (((ActStockView *)&actStock)->max > 0) {
        do {
            i = 0;
            if (count > 0) {
                do {
                    if (table[i] != 0 && calcBattleChk((ObjectTask *)table[i]) != 0
                        && (*(unsigned int *)((ObjectTask *)table[i])->work & 0x10) == 0
                        && calcWctDec((ObjectTask *)table[i]) == 0) {
                        ObjectTask *u = (ObjectTask *)table[i];
                        unsigned int *w = (unsigned int *)u->work;
                        ((ObjectTask **)((ActStockView *)&actStock)->a)[n] = u;
                        *w |= 0x10;
                        n = n + 1;
                        if (n >= ((ActStockView *)&actStock)->max)
                            goto done;
                    }
                    i++;
                } while (i < count);
            }
        } while (n < ((ActStockView *)&actStock)->max);
    }
done:
    q = (int *)&actStock;
    printf(D_00A43D10, q[6], n);
    i = 0;
    if (n > 0) {
        q = (int *)&actStock;
        do {
            printf(D_00A43D30, i, *q, calcUPGet((ObjectTask *)*q)->charaId);
            q++;
            i++;
        } while (i < n);
    }
    printf(D_00A43D40);
}

extern const char D_00A43D48[];
extern const char D_00A43D50[];
extern const char D_00A43D58[];
extern CalcUnitParam *calcUPGet(ObjectTask *unit);
void actStockMake(void);

ObjectTask *actStockPick(void)
{
    int p;

    ((ActStockView *)&actStock)->e = 0;
    do {
        if (((ActStockView *)&actStock)->max <= ((ActStockView *)&actStock)->c && ((ActStockView *)&actStock)->b[0] == 0 && ((ActStockView *)&actStock)->b[1] == 0) {
            actStockMake();
            ((ActStockView *)&actStock)->e = 1;
        }
        if (((ActStockView *)&actStock)->b[0] != 0) {
            p = ((ActStockView *)&actStock)->b[0];
            ((ActStockView *)&actStock)->e = 2;
            ((ActStockView *)&actStock)->b[0] = 0;
            printf(D_00A43D48);
            if (((ActStockView *)&actStock)->c > 0)
                ((ActStockView *)&actStock)->a[((ActStockView *)&actStock)->c - 1] = 0;
        } else if (((ActStockView *)&actStock)->b[1] != 0) {
            p = ((ActStockView *)&actStock)->b[1];
            ((ActStockView *)&actStock)->b[1] = 0;
            ((ActStockView *)&actStock)->e = 2;
            printf(D_00A43D50);
            if (((ActStockView *)&actStock)->c > 0)
                ((ActStockView *)&actStock)->a[((ActStockView *)&actStock)->c - 1] = 0;
        } else {
            int c = ((ActStockView *)&actStock)->c;
            p = ((ActStockView *)&actStock)->a[c];
            if (c > 0)
                ((ActStockView *)&actStock)->a[c - 1] = 0;
            ((ActStockView *)&actStock)->c = c + 1;
        }
        if (p == 0 || (**(int **)(p + 0x10) & 0x80))
            p = 0;
    } while (p == 0);
    printf(D_00A43D58, p, calcUPGet((ObjectTask *)p)->charaId, ((ActStockView *)&actStock)->c, ((ActStockView *)&actStock)->max);
    ((ActStockView *)&actStock)->d = p;
    return (ObjectTask *)p;
}

int actStockRemove(int value, int *outIndex)
{
    int i, found;
    int b0;

    found = 0;
    b0 = ((ActStockView *)&actStock)->b[0];
    *outIndex = -1;
    if (b0 == value) {
        ((ActStockView *)&actStock)->b[0] = 0;
        found = 1;
    }
    if (((ActStockView *)&actStock)->b[1] == value) {
        ((ActStockView *)&actStock)->b[1] = 0;
        found = 1;
    }
    i = 0;
    if (((ActStockView *)&actStock)->max <= 0)
        goto done;
    if (((ActStockView *)&actStock)->a[0] == value) {
        ((ActStockView *)&actStock)->a[0] = 0;
        found = 1;
        *outIndex = 0;
        goto done;
    }
loop:
    i++;
    if (i >= ((ActStockView *)&actStock)->max)
        goto done;
    if (((ActStockView *)&actStock)->a[i] != value)
        goto loop;
    *outIndex = i;
    found = 1;
    ((ActStockView *)&actStock)->a[i] = 0;
done:
    return found;
}

/*
 * The current action-stock count actStockInit/actStockMake/actStockPick/
 * actStockRemove (still INCLUDE_ASM here) read and write through the
 * address this returns (data still asm-owned; original ELF symbol name).
 */
extern int actStock;

int *actStockGet(void)
{
    return &actStock;
}

/*
 * The battle help page index, cycled 0..2 (data still asm-owned, splat
 * name kept).
 */
extern int helpFlag;

int battleCtrlHelp(void)
{
    helpFlag = (helpFlag + 1) % 3;
    return 1;
}

int battleCtrlHelpGet(void)
{
    return helpFlag;
}

extern CalcUnitParam *calcUPGet(ObjectTask *unit);
extern int unitTblGet(int side, BattleUnit ***table);
extern int calcBoostChk(BattleUnit *unit, int mode);
extern int calcBoost(BattleUnit *unit, int mode);

extern int dataPadRead(int padNo);
extern const char D_00A43D78[];
void menuBoostDisp(int enabled);

int battleCtrlBoostPl(void)
{
    BattleUnit **table;
    int key;
    int idx;

    key = dataPadRead(1);
    if (dataPadRead(0) & 0xA) {
        menuBoostDisp(1);
        idx = -1;
        if (key & 0x80) {
            idx = 0;
        } else {
            if (key & 0x10) {
                idx = 1;
            } else if (key & 0x20) {
                idx = 2;
            }
        }
        unitTblGet(0, &table);
        if (idx != -1) {
            if (table[idx] != 0 && calcBoostChk(table[idx], 1) != 0) {
                printf(D_00A43D78, calcUPGet((ObjectTask *)table[idx])->charaId,
                       ((signed char *)calcUPGet((ObjectTask *)table[idx]))[0x3C]);
                calcBoost(table[idx], 1);
                return 1;
            }
        }
    } else {
        menuBoostDisp(0);
    }
    return 0;
}

extern int rnd(int max);
extern const char D_00A43D90[];

int battleCtrlBoostEn(void)
{
    BattleUnit **table;
    int count;
    int chance;
    int i;

    count = unitTblGet(1, &table);
    for (i = 0; i < count; i++) {
        if (table[i] != 0) {
            chance = ((signed char *)calcUPGet((ObjectTask *)table[i]))[CALC_PARAM_BOOST_CHANCE_OFF];
            if (calcBoostChk(table[i], 1) != 0 && rnd(100) < chance) {
                printf(D_00A43D90, calcUPGet((ObjectTask *)table[i])->charaId, chance,
                       ((signed char *)calcUPGet((ObjectTask *)table[i]))[CALC_PARAM_BOOST_LEVEL_OFF]);
                calcBoost(table[i], 1);
                return 1;
            }
        }
    }
    return 0;
}

int menuChk(void);
void curSelGrpSet(u32 group, int flags);
int curEnProcInit(ObjectTask **target, int side);
int curEnProc(int *arg0);
void tgtUnitSet(BattleUnit *unit);
int manInput(BattleUnit *unit, int *inp);
int convKey2unit(ObjectTask *arg0, void *arg1, void *arg2, int arg3);
int calcWpnUse(ObjectTask *unit, int idx);
int calcAp(ObjectTask *unit, int delta);

int battleCtrlMan(BattleUnit *unit, int *inp)
{
  int bit;
  int enemy;
  BattleUnit *tgt;
  int cmd[8];
  int ap;
  bit = *(int *) unit->work & 0x40;
  enemy = bit == 0;
  tgt = tgtUnitGet();
  if (inp[0] != 1)
  {
    if (menuChk() == 0)
    {
      pManUnit = unit;
      pActUnit = unit;
      curSelGrpSet(1, 0);
      curEnProcInit((ObjectTask **) &tgt, enemy);
      if (curEnProc((int *) &tgt) != 0)
      {
        CamEvent *ev = &D_00A57B80;
        ev->type = 8;
        ev->unit = pActUnit;
        ev->target = *(BattleUnit * volatile *) &tgt;
        ((void (*)(CamEvent *))camEventExec)(ev);
      }
      tgtUnitSet(tgt);
      ((Actor *) pTaskEnCur->work)->flags &= ~4;
    }
  }
  ap = manInput(unit, inp);
  if (inp[1] == 0x1a)
  {
    cmd[0] = 8;
    unitCmdListSet((ObjectTask *) unit, cmd);
    keyBuffClear();
    inp[0] = 0;
    inp[3] = inp[1];
    return 0;
  }
  if ((inp[1] != 0) && (inp[1] != 0x1b))
  {
    convKey2unit((ObjectTask *) unit, cmd, inp, (int) tgt);
    unitCmdListSet((ObjectTask *) unit, cmd);
    if (cmd[0] == 1)
    {
      calcWpnUse((ObjectTask *) unit, cmd[3]);
    }
    calcAp((ObjectTask *) unit, -ap);
  }
  return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/battle_init", manInput);

void camEventTurnStartSet(void)
{
    D_00A57B80.type = 6;
    D_00A57B80.unit = pActUnit;
    D_00A57B80.target = tgtUnitGet();
    D_00A57B80.param = (*(int *)*(void **)((char *)pActUnit + BATTLE_UNIT_WORK_OFF) >> 6) & 1;
    camEventExec(&D_00A57B80, pActUnit);
}

/*
 * The two command-list buffers cmdListSet/cmdListChange/unitCmdListSet/
 * cmdListCansel (still INCLUDE_ASM here) operate on (data still asm-owned,
 * original ELF symbol names); cmdListPtrGet is the only place both addresses are
 * taken together.
 */
extern int fifoCmd;
extern int cmdList;

void cmdListPtrGet(int **list, int **altList)
{
    *list = &fifoCmd;
    *altList = &cmdList;
}

typedef struct { int w[5]; } CmdListBody;

typedef struct { int unit; CmdListBody body; } CmdListEntry;

int fifoEdGet(void *fifo);
void fifoPush(void *fifo);

void cmdListSet(int unit, CmdListBody *body)
{
    int *fifo;
    CmdListEntry *list;
    CmdListEntry *e;

    cmdListPtrGet(&fifo, (int **)&list);
    e = (CmdListEntry *)((char *)(fifoEdGet(fifo) * sizeof(CmdListEntry)) + (int)list);
    e->unit = unit;
    e->body = *body;
    fifoPush(fifo);
}

int fifoStGet(void *fifo);
void cmdListChange(int unit, CmdListBody *body)
{
    int *fifo;
    CmdListEntry *list;
    CmdListEntry *e;

    cmdListPtrGet(&fifo, (int **)&list);
    e = (CmdListEntry *)((char *)(fifoStGet(fifo) * sizeof(CmdListEntry)) + (int)list);
    e->unit = unit;
    e->body = *body;
}

int keyCmd2tid(ObjectTask *unit, int key);
int keyCmd2wno(ObjectTask *unit, int key);
unsigned char *dataEthGet(int id);
void dataItmBoxDec(int id);

int convKey2unit(ObjectTask *arg0, void *arg1, void *arg2, int arg3) {
    int *out = (int *)arg1;
    int *in = (int *)arg2;
    int tid;
    int v;

    if (in[1] != 0) {
        if (in[1] == 26) {
            out[0] = 8;
        } else if (in[1] < 20) {
            tid = keyCmd2tid(arg0, in[1]);
            out[0] = 1;
            out[2] = arg3;
            out[1] = tid;
            out[3] = keyCmd2wno(arg0, in[1]);
            if (tid == 0) {
                printf("** CMD ERR -> %d\n", in[1]);
            }
        } else {
            switch (in[1]) {
            case 20:
                out[2] = arg3;
                out[0] = 2;
                v = in[2];
                out[3] = v;
                out[1] = *(short *)(dataEthGet(v) + 6);
                break;
            case 21:
                out[2] = arg3;
                out[0] = 3;
                {
                    int w = in[2];
                    out[1] = 0;
                    out[3] = w;
                }
                dataItmBoxDec(in[2]);
                break;
            case 22:
                out[0] = 4;
                break;
            case 23:
                out[0] = 5;
                out[1] = in[2];
                break;
            case 24:
                out[0] = 6;
                out[1] = in[2];
                break;
            case 25:
                out[2] = arg3;
                out[0] = 3;
                {
                    int w = in[2];
                    out[1] = 2;
                    out[3] = w;
                }
                dataItmBoxDec(in[2]);
                dataItmBoxDec(in[2]);
                break;
            }
        }
    }
    return 1;
}

int unitCmdListSet(ObjectTask *unit, int *cmd)
{
    CmdListBody body;
    CalcUnitParam *up;
    int type;
    int tec;
    int *work;

    up = calcUPGet(unit);
    type = cmd[0];
    if ((unsigned)(type - 1) < 2) {
        body.w[0] = 0xE;
        body.w[1] = cmd[1];
        body.w[2] = cmd[2];
        body.w[3] = cmd[3];
        body.w[4] = type;
        cmdListSet((int)unit, &body);
        tec = *(unsigned char *)dataTecGet(cmd[1]) & 3;
        if (tec != 0) {
            if (*(unsigned char *)dataTecGet(cmd[1]) & 0x10) {
                body.w[0] = 0x2C;
            } else {
                body.w[0] = 0xF;
            }
            cmdListSet((int)unit, &body);
        }
        if (tec == 0) {
            if (!(*(int *)unit->work & 0x100)) {
                body.w[0] = 0x11;
                cmdListSet((int)unit, &body);
                ((union { int i; void *p; } *)unit->work)->i |= 0x100;
            }
        }
        *(unsigned int *)unit->work |= 0x200;
        if (*(signed char *)((char *)dataTecGet(cmd[1]) + 0x11) == 2) {
            work = unit->work;
            *work |= 0x1000;
        }
    } else if (type == 3) {
        body.w[0] = 0xE;
        body.w[1] = cmd[1];
        body.w[2] = cmd[2];
        body.w[3] = cmd[3];
        body.w[4] = type;
        cmdListSet((int)unit, &body);
    } else if (type == 8) {
        if (*(int *)unit->work & 0x100) {
            body.w[0] = 0x12;
            body.w[1] = *(short *)up->unmodeled_44;
            cmdListSet((int)unit, &body);
        } else if (*(int *)unit->work & 0x200) {
            body.w[0] = 0x10;
            body.w[1] = *(short *)up->unmodeled_44;
            cmdListSet((int)unit, &body);
        }
        *(int *)unit->work &= ~0x1300;
    }
    cmdListSet((int)unit, (CmdListBody *)cmd);
    return 1;
}

int fifoNumGet(void *fifo);
void fifoPop(void *fifo);

void cmdListCansel(int unit)
{
    int *fifo;
    CmdListEntry *list;
    CmdListEntry *e;

    cmdListPtrGet(&fifo, (int **)&list);
    while (fifoNumGet(fifo) != 0) {
        e = (CmdListEntry *)((char *)(fifoStGet(fifo) * sizeof(CmdListEntry)) + (int)list);
        if (e->unit != unit || e->body.w[0] == 8)
            break;
        fifoPop(fifo);
    }
}

/*
 * Menu map-height toggle (ov01/tu009 src/ov01/menu.c, not yet published;
 * forward-declared here until that TU's header exists).
 */
extern void menuMapHSet(int enable);

/*
 * Enemy AI attack-selection entry point (ov01/tu006 src/ov01/cmd.c, still
 * asm there; forward-declared here until that TU publishes its header).
 * cmd.c's thinkUnitAtkExec passes its argument straight to calcUPGet, whose
 * published prototype (src/ov01/unit_cmd.h) takes ObjectTask *.
 */
extern void thinkUnitAtkExec(ObjectTask *unit);

int battleCtrlThink(ObjectTask *unit)
{
    menuMapHSet(1);
    thinkUnitAtkExec(unit);
    return 0;
}

extern struct { BattleUnit *unit; int cmd; } statUnit;

extern unsigned char *scenarioDstGet(int offset);
extern int scenarioBatExec(void);
extern int busyUnitChk(int side);
extern int stat1Proc(void *p);
extern int stat2Proc();
extern int battleExecDead(void);
extern void menuTimeNext(void);
extern void sndInit2(void);
extern int battleExecNorm(void);
extern void unitCmdSet(ObjectTask *unit, int *cmd);
extern int D_00A57B78;
struct BatCtrlX { int unk0; int unk4; int unk8; int unkC; };

void battleExec(void)
{
    int cmd[8];

    switch (((struct BatCtrlX *)&batCtrl)->unk4) {
    case 1:
        battleExecNorm();
        break;
    case 2:
        if (scenarioDstGet(0) == 0) {
            ((struct BatCtrlX *)&batCtrl)->unk4 = 8;
            ((struct BatCtrlX *)&batCtrl)->unk8 = 1;
            break;
        }
        ((struct BatCtrlX *)&batCtrl)->unk4 = 3;
        {
            int *dst = (int *)scenarioDstGet(0);
            D_00A57B78 = dst[2];
        }
    case 3:
        if (scenarioBatExec() != 0) {
            scenarioBatInit();
            ((struct BatCtrlX *)&batCtrl)->unk4 = 1;
            if (battleExecNorm() == 0) {
                if (pExecUnit != 0) {
                    cmd[0] = 7;
                    unitCmdSet((ObjectTask *)pExecUnit, cmd);
                }
            }
        }
        break;
    case 4:
        sndInit2();
        if (stat1Proc(&statUnit) != 0) {
            sndInit2();
            ((struct BatCtrlX *)&batCtrl)->unk4 = 7;
            ((struct BatCtrlX *)&batCtrl)->unk8 = 5;
            break;
        }
        ((struct BatCtrlX *)&batCtrl)->unk4 = 5;
    case 5:
        if (battleExecDead() != 0)
            break;
    case 6:
        menuTimeNext();
        sndInit2();
        if (stat2Proc(&statUnit) != 0) {
            ((struct BatCtrlX *)&batCtrl)->unk4 = 7;
            ((struct BatCtrlX *)&batCtrl)->unk8 = 1;
            break;
        }
        ((struct BatCtrlX *)&batCtrl)->unk4 = 1;
        break;
    case 7:
        if (busyUnitChk(2) > 0)
            break;
        ((struct BatCtrlX *)&batCtrl)->unk4 = ((struct BatCtrlX *)&batCtrl)->unk8;
        break;
    case 8:
        if (((int *)pExecUnit->work)[0] & 2)
            break;
        ((struct BatCtrlX *)&batCtrl)->unk4 = ((struct BatCtrlX *)&batCtrl)->unk8;
        break;
    case 9:
        if (busyUnitChk(0) > 0)
            break;
        if (((struct BatCtrlX *)&batCtrl)->unkC-- > 0)
            break;
        ((struct BatCtrlX *)&batCtrl)->unk4 = ((struct BatCtrlX *)&batCtrl)->unk8;
        break;
    }
}

extern int unitMotGet(BattleUnit *unit);
extern void menuCritDispOff(void);
extern void thinkTurnEndExec(BattleUnit *unit);
extern void calcTurnEnd(BattleUnit *unit);
extern void unitCmdSet(ObjectTask *unit, int *cmd);
extern int D_00A5756C;

int battleExecNorm(void)
{
    int *fifo;
    CmdListEntry *list;
    CmdListEntry *e;
    BattleUnit *unit;
    int *cmd;
    int cmdNo;

    cmdListPtrGet(&fifo, (int **)&list);
    if (fifoNumGet(fifo) == 0)
        return 0;
    e = (CmdListEntry *)((char *)(fifoStGet(fifo) * sizeof(CmdListEntry)) + (int)list);
    unit = (BattleUnit *)e->unit;
    cmd = e->body.w;
    if (unitMotGet(unit) == 2 && (*(int *)((char *)unit->actor + 0x6F0) & 2))
        return 0;
    if (unit != pExecUnit) {
        menuCritDispOff();
        statUnit.unit = 0;
        statUnit.cmd = 0;
    }
    cmdNo = *cmd;
    if ((unsigned)(cmdNo - 1) < 6) {
        statUnit.cmd = cmdNo;
        statUnit.unit = unit;
    }
    if (unit != 0 && !(*(int *)unit->work & 0x80)) {
        pExecUnit = unit;
        unitCmdSet((ObjectTask *)unit, cmd);
        if (*cmd == 8) {
            thinkTurnEndExec(unit);
            calcTurnEnd(unit);
            pLastUnit = pExecUnit;
            pExecUnit = 0;
        }
    }
    fifoPop(fifo);
    cmdNo = (int)pExecUnit;
    if (cmdNo != 0)
        goto set;
    menuCritDispOff();
    BC->unk4 = 7;
    BC->unk8 = 4;
    goto out;
set:
    D_00A5756C = 2;
out:
    return 1;
}

extern int unitCmdDeadSet(ObjectTask *unit);
extern void decoEffCall(ObjectTask *unit, int type);
extern void monsGainSet(int unit);
extern void menuStockDead(int n);
extern int menuTimeGet(void);
extern int menuTimeXGet(void);
extern void menuCritDispOn(int mode);
extern void *dataSpecDataGet(int id, int tid);
extern const char D_00A43E78[];
extern void UmnDataBaseMonsterSet(void);

int battleExecDead(void)
{
  BattleUnit **table;
  int count;
  int i;
  int flag;
  const char *new_var;
  int stock;
  ObjectTask *linked;
  char *bc;
  CamEvent *ev;
  const char *fmt;
  flag = 0;
  i = 0;
  count = unitTblGet(2, &table);
  if (count > 0)
  {
    bc = (char *) (&batCtrl);
    ev = &D_00A57B80;
    ;
    do
    {
      if (table[i] != 0)
      {
        if ((calcBattleChk((ObjectTask *) table[i]) == 0) || (((short) calcUPGet((ObjectTask *) table[i])->currentHp) == 0))
        {
          if ((actStockRemove((int) table[i], &stock) != 0) && (stock > 0))
          {
            menuStockDead(stock);
          }
        }
        if (unitCmdDeadSet((ObjectTask *) table[i]) != 0)
        {
          flag = 1;
          if ((*((int *) ((ObjectTask *) table[i])->work)) & 0x40)
          {
            decoEffCall((ObjectTask *) table[i], 0x68);
            monsGainSet((int) table[i]);
            {
              short charaId = calcUPGet((ObjectTask *) table[i])->charaId;
              if ((((charaId != 0x22) && (charaId != 0x24)) && (charaId != 0x99)) && (((unsigned) (charaId - 0x6F)) >= 0x28))
              {
                UmnDataBaseMonsterSet();
              }
            }
            if (menuTimeGet() == 3)
            {
              int mode = menuTimeXGet();
              if (mode == 10)
              {
                mode = 3;
              }
              else
              {
                mode = ((mode ^ 4) != 0) ? (1) : (2);
              }
              menuCritDispOn(mode);
              *((int *) (bc + 0xC)) = 0x3C;
            }
            else
            {
              *((int *) (bc - -0xC)) = 0;
            }
          }
          linked = *((ObjectTask **) (((char *) calcUPGet((ObjectTask *) table[i])) + 0x150));
          if (linked != 0)
          {
            short cid = calcUPGet(linked)->charaId;
            void *spec = dataSpecDataGet(cid, *((int *) (((char *) calcUPGet((ObjectTask *) table[i])) + 0x154)));
            if (spec != 0)
            {
              if (((*((int *) (((char *) calcUPGet((ObjectTask *) table[i])) + 0x154))) != 0x61) || (calcUPGet((ObjectTask *) table[i])->flags & 0x2000))
              {
                *((unsigned short *) (((char *) spec) + 0xA)) = (*((unsigned short *) (((char *) spec) + 0xA))) + 1;
              }
            }
          }
          new_var = D_00A43E78;
          {
            short cid2 = calcUPGet((ObjectTask *) table[i])->charaId;
            int tid = *((int *) (((char *) calcUPGet((ObjectTask *) table[i])) + 0x154));
            printf(new_var, cid2, tid);
          }
          {
            BattleUnit *ty = (BattleUnit *) 0x13;
            *(BattleUnit **) &ev->type = ty;
            ev->unit = table[i];
            camEventExec(ev, ty);
          }
        }
      }
      i++;
    }
    while (i < count);
  }
  if (flag != 0)
  {
    ((struct BatCtrlL *) (&batCtrl))->unk4 = 9;
    ((struct BatCtrlL *) (&batCtrl))->unk8 = 6;
  }
  else
  {
    ((struct BatCtrlL *) (&batCtrl))->unk4 = 6;
  }
  return flag;
}

INCLUDE_ASM("asm/nonmatchings/ov01/battle_init", stat1Proc);

extern void unitCmdClear(ObjectTask *unit);
extern int calcHp(ObjectTask *unit, int v);
extern int dmgValPut(int, BattleUnit **);
extern const char D_00A43EC0[];
extern const char D_00A43ED0[];
extern const char D_00A43EA8[];
extern int itemGrpChk(int grp, int id);
extern int dataGainGet(int id);
extern void dataBoxInc(int a, int b);
typedef struct { unsigned char c[12]; } Str12;

int stat2Proc(void)
{
  int cmd[8];
  struct 
  {
    BattleUnit *unit;
    int pad;
    int dmg;
    int flags;
    int more[8];
  } rec;
  BattleUnit **table;
  BattleUnit *unit;
  int count;
  int i;
  int ret;
  int dmg;
  unsigned char *g;
  unsigned char b17;
  unsigned char b18;
  unsigned short t;
  char *name;
  ret = 0;
  i = 0;
  count = unitTblGet(2, &table);
  if (count > 0)
  {
    do
    {
      unit = table[i];
      dmg = 0;
      if (unit != 0)
      {
        if ((*((int *) unit->work)) & 0x80)
        {
          if (calcStatGet((ObjectTask *) unit, 2, 0x4000) != 0)
          {
            dmg = *((short *) calcUPGet((ObjectTask *) unit));
            calcStatReset((ObjectTask *) unit, 2, 0x4000, 0);
            goto block_27;
          }
          if (calcBackLineChk((ObjectTask *) unit, 0) != 0)
          {
            goto block_17;
          }
          if (calcStatGet((ObjectTask *) unit, 3, 0x800) == 0)
          {
            goto block_17;
          }
          dmg = ((short) (*((unsigned short *) calcUPGet((ObjectTask *) unit)))) / 2;
          calcStatReset((ObjectTask *) unit, 3, 0x800, 0);
          goto block_27;
          block_17:
          if ((*((int *) unit->work)) & 0x40)
          {
            if (calcStatGet((ObjectTask *) unit, 1, 1) != 0)
            {
              calcStatReset((ObjectTask *) unit, 1, 1, 0);
              calcStatSet((ObjectTask *) unit, 1, 0x100, 0);
              goto block_27;
            }
            if (((*((int *) unit->work)) & 0x40) && (calcStatGet((ObjectTask *) unit, 7, 4) != 0))
            {
              calcStatReset((ObjectTask *) unit, 7, 4, 0);
              calcStatSet((ObjectTask *) unit, 1, 0x100, 0);
              g = (unsigned char *) dataGainGet(*((short *) (((char *) calcUPGet((ObjectTask *) unit)) + 0x38)));
              b18 = g[0xF];
              b17 = g[0x12];
              if (itemGrpChk(b18, b17) == 0)
              {
                b18 = 0;
                b17 = 0;
              }
              printf(D_00A43EA8, b18, b17);
              if (b17 != 0)
              {
                name = (char *) itemNameGet(b18, b17);
                if (name != 0)
                {
                  *((Str12 *) msgWork.text) = *((Str12 *) D_00A43EC0);
                  strcat(msgWork.text, name);
                  strcat(msgWork.text, D_00A43ED0);
                  battleMsg((int) (&msgWork));
                  dataBoxInc(b18, b17);
                  goto block_27;
                }
                goto next;
              }
              goto block_27;
            }
            else
            {
              goto block_27;
            }
          }
          else
          {
            goto block_27;
          }

          block_27:
          if (dmg != 0)
          {
            ret = 1;
            unitCmdClear((ObjectTask *) unit);
            cmd[0] = 0x28;
            unitCmdSet((ObjectTask *) unit, cmd);
            calcHp((ObjectTask *) unit, dmg);
            rec.dmg = dmg;
            rec.flags = 0x802;
            rec.unit = unit;
            dmgValPut(0, &rec.unit);
          }

          block_29:
          calcStatReset((ObjectTask *) unit, 7, 4, 0);

        }
        else
        {
          goto block_29;
        }
      }
      next:
      i += 1;

    }
    while (i < count);
  }
  return ret;
}

extern int keyBuffIdx;
int keyBuffSet(ObjectTask *unit, int key);
int keyBuffChk(ObjectTask *unit, int *out);
int keyCmdChk(ObjectTask *unit, BattleUnit *tgt, int cmd);

int keyCmdGet(ObjectTask *unit)
{
    int buf[4];
    int key;
    int cmd;
    int res;

    if (*(unsigned short *)((char *)calcUPGet(unit) + 0x1C) & 0x40) {
        if (((signed char *)calcUPGet(unit))[0x3A] < 3) {
            return 0x1A;
        }
    } else {
        if (((signed char *)calcUPGet(unit))[0x3A] < 2) {
            return 0x1A;
        }
        if ((*(int *)unit->work & 0x1000) && calcTuigekiChk(unit) == 0) {
            return 0x1A;
        }
    }
    if (dataPadRead(0) & 0xA) {
        return 0;
    }
    key = dataPadRead(1);
    if (key == 0) {
        return key;
    }
    if (key == 0x40) {
        cmd = 0x1A;
        keyBuffClear();
    } else {
    res = keyBuffSet(unit, key & 0xFFFF);
    if (res == 0) {
        return res;
    }
    cmd = keyBuffChk(unit, buf);
    if (cmd == 0x1B) {
        if (keyBuffIdx > 0) {
            keyBuffIdx--;
        }
        return cmd;
    }
    res = keyCmdChk(unit, tgtUnitGet(), cmd);
    if (res != 0) {
        if (keyBuffIdx > 0) {
            keyBuffIdx--;
        }
        if (res == 1) {
            sndSysSePlay(5);
        }
        cmd = 0x1B;
    }
    }
    return cmd;
}

typedef struct {
    unsigned short *keys;
    int arg;
    int cmd;
} KeyCmdEnt;
extern KeyCmdEnt keyCmdTblCha[];
extern int keyCmd2wno(ObjectTask *unit, int key);
extern char *dataWpnGet(int no);

int keyCmdApGet(ObjectTask *arg0, int arg1)
{
    int result = 0;
    CalcUnitParam *up;
    int i;
    int wpn;
    KeyCmdEnt *tbl;
    if (arg1 == 0 || arg1 == 0x1B || arg1 == 0x1A) {
        return 0;
    }
    if (arg1 < 0x14) {
        up = calcUPGet(arg0);
        if (up->flags & 0x40) {
            if (arg1 != 0x13) {
                wpn = keyCmd2wno(arg0, arg1);
                if (wpn != -1) {
                    up = calcUPGet(arg0);
                    result = ((signed char *)dataWpnGet(up->sefSetupParams[wpn]))[0xf];
                }
            } else {
                result = 6;
            }
        } else {
            tbl = keyCmdTblCha;
            i = 0;
            for (;;) {
                KeyCmdEnt *ent = (KeyCmdEnt *)((char *)(i * 12) + (int)tbl);
                if (ent->keys == 0) {
                    break;
                }
                if (ent->cmd == arg1) {
                    result = ent->arg;
                    break;
                }
                i++;
            }
        }
    } else {
        switch (arg1) {
            case 0x16:
            case 0x18:
                result = 2;
                break;
            case 0x15:
                result = 3;
                break;
            case 0x14:
            case 0x17:
                result = 4;
                break;
            case 0x19:
                result = 6;
                break;
        }
    }
    return result;
}

extern const char D_00A43EF8[];
extern int calcStatGet(BattleUnit *unit, int stat, int mask);
extern int calcWpnEnableChk(ObjectTask *unit, int wno);
extern int calcSpecChk(ObjectTask *unit, int tid);
extern int calcTuigekiChk(ObjectTask *unit);
extern int calcAtkChk(ObjectTask *unit, BattleUnit *tgt, int tid);

int keyCmdChk(ObjectTask *unit, BattleUnit *tgt, int cmd)
{
    int tid;

    switch (cmd) {
    case 0:
    case 27:
        return 2;
    default:
        if (tgt == 0) {
            printf(D_00A43EF8);
            return 2;
        }
    }
    tid = keyCmd2tid(unit, cmd);
    if (tid == 0) {
        return 2;
    }
    if (!(calcUPGet(unit)->flags & 0x40)) {
        int r;
        if (cmd >= 0x11) {
            if (calcTuigekiChk(unit) == 0) {
                return 2;
            }
            if (calcSpecChk(unit, tid) != 0) {
                goto ap;
            }
            return 1;
        } else if (cmd >= 0xB) {
            if (calcStatGet((BattleUnit *)unit, 0, 1) == 0) {
                goto spec2;
            }
            if ((unsigned int)(cmd - 0xB) < 8) {
                return 1;
            }
spec2:
            if (calcSpecChk(unit, tid) != 0) {
                goto ap;
            }
            return 1;
        } else {
            if (cmd == 7) {
                if (calcStatGet((BattleUnit *)unit, 0, 1) != 0) {
                    return 1;
                }
                if (calcWpnEnableChk(unit, 1) != 0) {
                    goto ap;
                }
                return 1;
            } else {
                if (calcStatGet((BattleUnit *)unit, 0, 4) != 0) {
                    if (cmd == 1 || cmd == 3 || cmd == 4 || cmd == 6) {
                        return 1;
                    }
                }
                if (calcStatGet((BattleUnit *)unit, 0, 2) != 0) {
                    if (cmd == 2 || cmd == 5 || cmd == 6 || cmd == 4) {
                        return 1;
                    }
                }
                goto ap;
            }
        }
        if (r != 0) {
            goto ap;
        }
        return 1;
    } else {
        int wno;
        wno = keyCmd2wno(unit, cmd);
        if (wno == -1) {
            return 2;
        }
        if (calcWpnEnableChk(unit, wno) == 0) {
            return 1;
        }
        if (calcStatGet((BattleUnit *)unit, 4, 4) != 0) {
            if (cmd == 1 || cmd == 3 || cmd == 4 || cmd == 0xB || cmd == 6 || cmd == 8) {
                return 1;
            }
        }
        if (calcStatGet((BattleUnit *)unit, 4, 2) != 0) {
            if (cmd == 2 || cmd == 5 || cmd == 6 || cmd == 0xC || cmd == 4 || cmd == 9) {
                return 1;
            }
        }
        if (calcStatGet((BattleUnit *)unit, 4, 1) != 0) {
            if (cmd == 7 || cmd == 9 || cmd == 8 || cmd == 0xA || cmd == 0xC || cmd == 0xB) {
                return 1;
            }
        }
    }

ap:
    {
        int r = keyCmdApGet(unit, cmd);
        if (((signed char *)calcUPGet(unit))[0x3A] < r) {
            return 1;
        }
        return calcAtkChk(unit, tgt, tid) == 0 ? 1 : 0;
    }
}

extern int calcDblActChk(ObjectTask *unit);

int keyCmd2tid(ObjectTask *arg0, int arg1)
{
  s16 var_16;
  s16 var_19;
  int wno;
  var_19 = 0;
  if (!(calcUPGet(arg0)->flags & 0x40))
  {
    if (arg1 >= 0x11)
    {
      wno = arg1 - 0x11;
      wno = (wno == 1) ? 4 : 2;
      var_19 = *((short *) ((((char *) calcUPGet(arg0)) + (wno * 2)) + 0x82));
    }
    else
      if (arg1 >= 0xB)
    {
      var_19 = *((short *) ((((char *) calcUPGet(arg0)) + (arg1 * 2)) + 0x6C));
    }
    else
      if (arg1 == 7)
    {
      if (calcUPGet(arg0)->flags & 0x10)
      {
        var_16 = calcUPGet(arg0)->sefSetupParams[1];
        if (var_16 != 0)
        {
          var_19 = *((short *) (dataWpnGet(var_16) + 0x14));
        }
      }
    }
    else
      if (arg1 != 0)
    {
      var_19 = *((short *) ((((char *) calcUPGet(arg0)) + (arg1 * 2)) + 0x74));
    }
  }
  else
  {
    wno = keyCmd2wno(arg0, arg1);
    if (wno != (-1))
    {
      var_16 = *((short *) ((((char *) calcUPGet(arg0)) + (wno * 2)) + 0x5E));
      if (var_16 != 0)
      {
        if ((arg1 == 19) && calcDblActChk(arg0))
        {
          var_19 = *((short *) (dataWpnGet(var_16) + 0x18));
        }
        else
          if (((arg1 == 3) || (arg1 == 5)) || (arg1 == 10))
        {
          var_19 = *((short *) (dataWpnGet(var_16) + 0x16));
        }
        else
        {
          var_19 = *((short *) (dataWpnGet(var_16) + 0x14));
        }
      }
    }
  }
  return var_19;
}

extern int calcSameWpnChk(ObjectTask *unit);

int keyCmd2wno(ObjectTask *unit, int key)
{
    CalcUnitParam *up;
    int res = -1;

    up = calcUPGet(unit);
    if (!(up->flags & 0x40)) {
        res = key == 7;
    } else {
        switch (key) {
        case 1:
        case 3:
        case 6:
        case 8:
            res = 0;
            break;
        case 2:
        case 4:
        case 5:
        case 9:
            res = 1;
            break;
        case 7:
        case 10:
        case 11:
        case 12:
            res = 2;
            break;
        case 19:
            res = calcSameWpnChk(unit);
            break;
        }
    }
    if (res != -1 && calcUPGet(unit)->sefSetupParams[res] == 0) {
        res = -1;
    }
    return res;
}

/*
 * Buffered battle-input queue (data still asm-owned; original ELF symbol names):
 * keyBuffIdx is the current insertion index into the keyBuff queue.
 */
extern int keyBuffIdx;
extern int keyBuff[8];

void keyBuffClear(void)
{
    memset(keyBuff, 0, sizeof(keyBuff));
    keyBuffIdx = 0;
}

int keyBuffIdxGet(void)
{
    return keyBuffIdx;
}

extern u16 keyItemTblCha[4];
extern u16 keyItemTblAgws[4];
extern char D_00A43F60[];

int keyBuffSet(ObjectTask *arg0, int arg1)
{
    int temp_16;
    int var_5;
    int var_3;
    u16 *var_4;
    u16 var_2;

    temp_16 = arg1 & 0xFFFF;
    var_5 = 0;
    if (calcUPGet(arg0)->flags & 0x40) {
        var_4 = keyItemTblAgws;
    } else {
        var_4 = keyItemTblCha;
    }
    var_2 = *var_4;
    var_3 = 0;
    while (var_2 != 0) {
        if (temp_16 == var_2) {
            var_5 = 1;
            break;
        }
        var_3 += 1;
        var_2 = var_4[var_3];
    }
    if (var_5 == 0) {
        return 0;
    }
    if (keyBuffIdx < 8) {
        keyBuff[keyBuffIdx] = temp_16;
        keyBuffIdx += 1;
        return 1;
    }
    printf(D_00A43F60, &keyBuffIdx);
    return 0;
}

extern KeyCmdEnt keyCmdTblCha[];
extern KeyCmdEnt keyCmdTblAgws[];
extern char *dataWpnGet(int no);

int keyBuffChk(ObjectTask *unit, int *out)
{
    KeyCmdEnt *t;
    KeyCmdEnt *tbl;
    unsigned short *k;
    unsigned short *kk;
    int *b;
    int n;
    int i;
    int w;

    *out = 0;
    if (keyBuffIdx == 0) {
        return 0;
    }
    if (*(unsigned short *)((char *)calcUPGet(unit) + 0x1C) & 0x40) {
        tbl = keyCmdTblAgws;
    } else {
        tbl = keyCmdTblCha;
    }
    if (*(int *)tbl != 0) {
    t = tbl;
    do {
        k = t->keys;
        n = *k++;
        if (keyBuffIdx != n) {
            goto next;
        }
        for (i = 0; i < n; i++) {
            if (keyBuff[i] != k[i]) {
                break;
            }
        }
        if (i != n) {
            goto next;
        }
        if (*(unsigned short *)((char *)calcUPGet(unit) + 0x1C) & 0x40) {
            w = keyCmd2wno(unit, t->cmd);
            if (w == -1) {
                return 0x1B;
            }
            *out = dataWpnGet(*(short *)((char *)calcUPGet(unit) + w * 2 + 0x5E))[0xF];
        } else {
            *out = t->arg;
        }
        return t->cmd;
    next:
        t++;
    } while (*(int *)t != 0);
    }
    return 0x1B;
}

int busyUnitChk(int side)
{
    BattleUnit **units;
    int count = unitTblGet(side, &units);
    int busyCount = 0;
    int i;

    for (i = 0; i < count; i++) {
        BattleUnit *unit = units[i];

        if (unit != 0 && (unit->work->flags & 2) != 0) {
            busyCount++;
        }
    }
    return busyCount;
}

/*
 * Collects the units of one side that are still taking part in the battle
 * and returns how many there are. The test is on the flags word of the
 * unit's own actor (the same word transWepIn and camEventTurnStartSet
 * read): bit 0x8 must be set and bit 0x80 clear.
 * A null actUnits only counts them; otherwise the collected units are
 * stored in it in table order.
 */
int actUnitTblMake(int side, BattleUnit **actUnits)
{
    BattleUnit **table;
    BattleUnit **next;
    BattleUnit **out;
    BattleUnit *unit;
    u32 flags;
    int remaining;
    int count;
    int made;

    count = unitTblGet(side, &table);
    made = 0;
    if (count > 0) {
        next = table;
        remaining = count;
        out = actUnits;
        do {
            unit = *next;
            next++;
            if (unit != 0) {
                flags = unit->work->flags;
                if (!(flags & 0x80)) {
                    if (flags & 8) {
                        if (actUnits != 0) {
                            *out = unit;
                        }
                        out++;
                        made++;
                    }
                }
            }
            remaining--;
        } while (remaining != 0);
    }
    return made;
}

/*
 * Defined in ov01/tu004 src/ov01/calc.c; not yet published there.
 */
extern int rnd(int max);

BattleUnit *tgtUnitPick(BattleUnit **table, int count)
{
    if (count > 0) {
        return table[rnd(count - 1)];
    }
    return 0;
}

extern int batRetCode;
u8 cfEventGet(void);

int battleEndChk(void) {
    int *ret = &batRetCode;
    int mode;
    int live0;

    *ret = 0;
    if (busyUnitChk(2) > 0) {
        return 1;
    }
    mode = D_00A5756C;
    if (mode != 1) {
        return 1;
    }
    live0 = unitLiveNumGet2(0);
    if (live0 == 0) {
        *ret = 2;
        return 0;
    }
    if (unitLiveNumGet2(1) == 0) {
        *ret = mode;
        if (cfEventGet() & 0x10) {
            *ret = 3;
        }
        if (cfEventGet() & 0x20) {
            *ret = 3;
        }
        return 0;
    }
    if (escapeFlag == mode) {
        *ret = 3;
        return 0;
    }
    return 1;
}

/*
 * The battle result code consumed by phase transitions and shutdown
 * processing (data still asm-owned; original ELF symbol name).
 */
extern int batRetCode;

int battleRetCodeGet(void)
{
    return batRetCode;
}

void battleRetCodeSet(int retCode)
{
    batRetCode = retCode;
}

int thinkNoGet(void)
{
    return D_00A57C80;
}

void thinkNoSet(int thinkNo)
{
    D_00A57C80 = thinkNo;
}

/*
 * The party's per-slot attack assignment (src/main/party.h
 * PartyAttackPosition: that TU-local header cannot be included from here).
 * Only the 4-byte stride and the two fields plUnitTblGet copies are modeled.
 */
typedef struct PartyAttackSlot {
    unsigned short partyId;      /* +0x0 */
    signed char attackPosition;  /* +0x2 */
} PartyAttackSlot;

/*
 * plTbl (data still asm-owned; original ELF symbol name): three 8-byte
 * entries. plUnitTblGet writes only the first two fields of each; the
 * remaining four bytes are written by plUnitTblSet (still INCLUDE_ASM).
 */
typedef struct PlUnitEntry {
    short partyId;         /* +0x0 */
    short attackPosition;  /* +0x2 */
    unsigned char unmodeled_04[4];
} PlUnitEntry;
extern PlUnitEntry plTbl[3];

PlUnitEntry *plUnitTblGet(void) {
    PartyAttackSlot *slot;
    PlUnitEntry *entry;
    int i;

    slot = (PartyAttackSlot *)(PartyDataGet() + 0x30);
    entry = plTbl;
    for (i = 0; i < 3; i++) {
        entry->partyId = (short)slot->partyId;
        entry->attackPosition = (short)slot->attackPosition;
        slot++;
        entry++;
    }
    return plTbl;
}

typedef struct PartySlotView {
    unsigned char unmodeled_00[0x30];
    PartyAttackSlot slot[3]; /* +0x30 */
} PartySlotView;

void plUnitTblSet(int idx, int partyId, int attackPosition)
{
    PartySlotView *party = (PartySlotView *)PartyDataGet();

    party->slot[idx].partyId = partyId;
    party->slot[idx].attackPosition = attackPosition;
    plTbl[idx].partyId = partyId;
    plTbl[idx].attackPosition = attackPosition;
}

/*
 * GameLoopState (config/symbols/main.txt, data still asm-owned) is a
 * 0x2A030-byte record several TUs each model only where they touch it (for
 * example src/main/script.h's GameLoopState+0x29F40 event byte); the active
 * monster-set number monsSetNoGet/monsSetNoSet share lives at +0x29F90 as a
 * signed halfword. The rest of the record is not recovered here.
 */
typedef struct GameLoopMonsSetView {
    unsigned char unmodeled_00[0x29F90];
    short monsSetNo; /* +0x29F90 */
} GameLoopMonsSetView;
extern GameLoopMonsSetView GameLoopState;

int monsSetNoGet(void)
{
    return GameLoopState.monsSetNo;
}

void monsSetNoSet(int monsSetNo)
{
    GameLoopState.monsSetNo = monsSetNo;
}

/*
 * GameLoopState also carries the current map number at +0x29F92, directly
 * after the monster-set number above; the published GameLoopMonsSetView
 * cannot be extended to reach it, so this TU models it through its own
 * prefix view of the same object; the rest of the record is not recovered
 * here.
 */
typedef struct GameLoopMapNoPrefix {
    unsigned char unmodeled_00[0x29F92];
    short mapNo; /* +0x29F92 */
} GameLoopMapNoPrefix;

int mapNoGet(void)
{
    GameLoopMapNoPrefix *mapNoView = (GameLoopMapNoPrefix *)&GameLoopState;
    return mapNoView->mapNo;
}

void mapNoSet(int mapNo)
{
    GameLoopMapNoPrefix *mapNoView = (GameLoopMapNoPrefix *)&GameLoopState;
    mapNoView->mapNo = mapNo;
}

/*
 * GameLoopState also carries the encounter/event configuration bytes at
 * +0x29F96 and +0x29F97 (lbu/sb against 0x00362616/0x00362617, base
 * 0x338680 + 0x29F96/+0x29F97, the same base the monster-set view above
 * uses). The published GameLoopMonsSetView above cannot be extended to
 * reach these bytes, so this TU models them through their own prefix view
 * of the same object; the rest of the record is not recovered here.
 */
typedef struct GameLoopCfConfigPrefix {
    unsigned char unmodeled_00[0x29F96];
    u8 cfEncount; /* +0x29F96 */
    u8 cfEvent;   /* +0x29F97 */
} GameLoopCfConfigPrefix;

u8 cfEncountGet(void)
{
    GameLoopCfConfigPrefix *cfConfig = (GameLoopCfConfigPrefix *)&GameLoopState;
    return cfConfig->cfEncount;
}

void cfEncountSet(u8 cfEncount)
{
    GameLoopCfConfigPrefix *cfConfig = (GameLoopCfConfigPrefix *)&GameLoopState;
    cfConfig->cfEncount = cfEncount;
}

u8 cfEventGet(void)
{
    GameLoopCfConfigPrefix *cfConfig = (GameLoopCfConfigPrefix *)&GameLoopState;
    return cfConfig->cfEvent;
}

void cfEventSet(u8 cfEvent)
{
    GameLoopCfConfigPrefix *cfConfig = (GameLoopCfConfigPrefix *)&GameLoopState;
    cfConfig->cfEvent = cfEvent;
}

BattleUnit *actUnitGet(void)
{
    return pActUnit;
}

BattleUnit *lastUnitGet(void)
{
    return pLastUnit;
}

extern BattleUnit *pTgtUnit;
extern BattleUnit *pTgtUnit2;
ManWork *manWorkGet(void);

BattleUnit **tgtUnitAdrGet(void)
{
    int n = *(int *)manWorkGet();
    switch (n) {
    case 6: case 7: case 9: case 10: case 11:
        return &pTgtUnit2;
    }
    return &pTgtUnit;
}

BattleUnit *tgtUnitGet(void)
{
    return *tgtUnitAdrGet();
}

void tgtUnitSet(BattleUnit *unit)
{
    *tgtUnitAdrGet() = unit;
}

ManWork *manWorkGet(void)
{
    return &manWk;
}

extern int menuOpenChkBat(void);
extern int menuFlag;

int menuChk(void)
{
    if (menuOpenChkBat() != 0) {
        return 1;
    }
    return menuFlag == 1;
}

BatCtrl *batCtrlGet(void)
{
    return &batCtrl;
}

void scenarioPtrFuncSet(ScenarioPhaseFunc func)
{
    scenarioPtr.nextPhase = func;
    scenarioPtr.phaseTimer = 0;
}

void scenarioBatExecPhase10();
extern void sndInit2(void);
extern unsigned char scenarioDst[];
extern unsigned char scenarioSrc[];

void scenarioBatInit(void)
{
    sndInit2();
    scenarioPtr.source = (ScenarioStep *)scenarioSrc;
    scenarioPtr.count = 0;
    scenarioPtr.index = 0;
    scenarioPtr.dest = scenarioDst;
    scenarioPtrFuncSet(scenarioBatExecPhase10);
}

int scenarioSrcSet(const ScenarioStep *source)
{
    extern int D_00A57B64;
    ScenarioStep *destination = (ScenarioStep *)D_00A57B64;

    *destination = *source;
    return 1;
}

typedef struct ScenarioDstRec { int w[10]; } ScenarioDstRec;

int scenarioDstSet(ScenarioDstRec *src)
{
    if (scenarioPtr.count >= 13) {
        return 0;
    }
    ((ScenarioDstRec *)scenarioPtr.dest)[scenarioPtr.count] = *src;
    scenarioPtr.count++;
    return 1;
}

/*
 * The scenario source index (data still asm-owned, splat name kept); a
 * different object from the ScenarioBatState record above.
 */
extern int D_00A57B64;

int scenarioSrcGet(void)
{
    return D_00A57B64;
}

unsigned char *scenarioDstGet(int offset)
{
    int i = scenarioPtr.index + offset;

    if (i >= 13 || i >= scenarioPtr.count) {
        return 0;
    }
    return scenarioPtr.dest + i * 40;
}

int scenarioBatNext(void)
{
    int index;

    index = scenarioPtr.index + scenarioPtr.source->stride;
    scenarioPtr.index = index;
    if (index >= scenarioPtr.count) {
        scenarioPtr.index = scenarioPtr.count;
        return 1;
    }
    return 0;
}

extern short *dataTecGet(int id);
extern short *dataItmGet(int id);

void scenarioCam(void *arg0, int arg1)
{
  int new_var;
  if (calcUPGet(*((ObjectTask **) arg0))->flags & 0x40)
  {
    return;
  }
  if (((int *) arg0)[1] == 3)
  {
    unsigned char v;
    D_00A57B80.unit = *((BattleUnit **) arg0);
    D_00A57B80.type = 0x14;
    D_00A57B80.target = *((BattleUnit **) arg1);
    v = *((unsigned char *) (((char *) dataItmGet(((int *) arg0)[3])) + 8));
    D_00A57B80.param = v;
    if (((int *) arg0)[2] != 0)
    {
      D_00A57B80.param = v | 0x10;
    }
    ((void (*)(CamEvent *)) camEventExec)(&D_00A57B80);
    return;
  }
  else
  {
    signed char cat = *((signed char *) (((char *) dataTecGet(((int *) arg0)[2])) + 0x11));
    if (((cat != 2) && (cat != 6)) && (!(0x10 & (*((unsigned char *) dataTecGet(((int *) arg0)[2]))))))
    {
      int k = (*((unsigned char *) dataTecGet(((int *) arg0)[2]))) & 3;
      if (k == 0)
      {
        D_00A57B80.type = 2;
        D_00A57B80.unit = *((BattleUnit **) arg0);
        D_00A57B80.param = ((int *) arg0)[2];
        D_00A57B80.target = *((BattleUnit **) arg1);
        ((void (*)(CamEvent *)) camEventExec)(&D_00A57B80);
      }
      else if (k == 1)
      {
        D_00A57B80.type = 3;
        D_00A57B80.unit = *((BattleUnit **) arg0);
        D_00A57B80.param = ((int *) arg0)[2];
        D_00A57B80.target = *((BattleUnit **) arg1);
        ((void (*)(CamEvent *)) camEventExec)(&D_00A57B80);
      }
    }
  }
}

extern void (*D_00A57B68)(int, unsigned char *);
void scenarioBatEnd(void);

int scenarioBatExec(void)
{
    unsigned char *dst = scenarioDstGet(0);
    int src = ((int (*)(int))scenarioSrcGet)(0);

    if (dst != 0) {
        D_00A57B68(src, dst);
        if (scenarioDstGet(0) != 0) {
            return 0;
        }
    }
    scenarioBatEnd();
    return 1;
}

int sefCheckLoad(void);
int dataCdSync(void);
void dataSndSeLoad2(int unit);

int scenarioBatDataSync(int *sourceIndex)
{
    if (sefCheckLoad() != 0) {
        return 0;
    }
    if (dataCdSync() > 0) {
        return 0;
    }
    dataSndSeLoad2(*sourceIndex);
    return 1;
}

int etehrCVGet(int *arg0) {
    short charaId;
    int result = 4;
    unsigned int val_8;

    charaId = calcUPGet((ObjectTask *)arg0[0])->charaId;

    switch (charaId) {
        case 3: {
            unsigned int tmp = (unsigned int)arg0[2] - 0x236;
            if (tmp < 4U) {
                result = 5;
            }
            break;
        }
        case 6: {
            unsigned int tmp = (unsigned int)arg0[2];
            switch (tmp) {
                case 596:
                    result = 5;
                    break;
                case 599:
                    result = 6;
                    break;
            }
            break;
        }
        case 7: {
            unsigned int tmp = (unsigned int)arg0[2] - 0x262;
            if (tmp < 2U) {
                result = 5;
            }
            break;
        }
    }

    return result;
}

extern void EXM_SetDirectionalWind(Vector4 *dir);
extern void EXM_SetShakePower(float power);
extern void EXM_SetShakeTime(float time);

void hairWindOn(BattleUnit *unit)
{
    BattleUnit **table;
    BattleUnit **p;
    BattleUnit *u;
    int count;
    int n;
    Vector4 dir;

    dir.x = 0.0f;
    dir.y = 1.0f;
    dir.z = 0.0f;
    dir.w = 0.03f;
    EXM_SetDirectionalWind(&dir);
    EXM_SetShakePower(1.0f);
    EXM_SetShakeTime(0.75f);
    n = unitTblGet(0, &table);
    if (n > 0) {
        count = n;
        p = table;
        do {
            u = *p++;
            if (u != 0 && u != unit) {
                char *q = *(char **)((char *)u + 0x14);
                *(int *)(q + 0x6F0) |= 0x100000;
            }
        } while (--count != 0);
    }
}

extern void EXM_StopWind(void);

void hairWindOff(BattleUnit *unit)
{
    BattleUnit **table;
    BattleUnit **p;
    BattleUnit *u;
    int count;
    int n;

    EXM_StopWind();
    n = unitTblGet(0, &table);
    if (n > 0) {
        count = n;
        p = table;
        do {
            u = *p++;
            if (u != 0 && u != unit) {
                char *q = *(char **)((char *)u + 0x14);
                *(int *)(q + 0x6F0) &= ~0x100000;
            }
        } while (--count != 0);
    }
}

extern int D_00A57B6C[];
void scenarioBatExecPhase20();
void scenarioCam(void *, int);
extern void statEffOff(ObjectTask *unit);
extern short *dataTecGet(int id);
extern short *dataItmGet(int id);
void sefLoadEffect(int, int);

void scenarioBatExecPhase10(ScenarioBatExec *arg0, int arg1)
{
    short var_16;
    int temp_2;

    if (D_00A57B6C[0] == 0) {
        statEffOff(arg0->unit);
        scenarioCam(arg0, arg1);
        if (*(int *)((char *)arg0 + 0x4) != 3) {
            var_16 = *(short *)((char *)dataTecGet(*(int *)((char *)arg0 + 0x8)) + 0x16);
        } else {
            var_16 = *(short *)((char *)dataItmGet(*(int *)((char *)arg0 + 0xC)) + 0x10);
        }
        sefLoadEffect(var_16, calcUPGet((ObjectTask *)arg0->unit)->charaId);
        temp_2 = sndConvSpid(arg0->unit, *(int *)((char *)arg0 + 0x4), *(int *)((char *)arg0 + 0x8), *(int *)((char *)arg0 + 0xC));
        if (sndSeRegChk(arg0->unit, temp_2) == 0) {
            dataSndSeLoad(arg0->unit, temp_2);
        }
    }
    scenarioPtrFuncSet(scenarioBatExecPhase20);
    scenarioBatExecPhase20(arg0, arg1);
}

extern int sndSysSePlay(int soundId);
extern void sndSeTransPlay(int unit, int seId, int volume);
extern void dataSndSeLoad(int unit, int seId);
void scenarioBatExecPhase25(void *arg0, int arg1);
int dataFileLoadWepSp(int, int);

void scenarioBatExecPhase20(ScenarioBatExec *arg0, int arg1) {
    int *ap = (int *)arg1;
    int temp_17;
    int t;
    int var_5;
    int temp_2_2;
    int temp_4_2;

    int state = *(int *)((char *)arg0 + 0x4);
    if (state == 2) {
        /* phase 2 */
        switch (scenarioPtr.phaseTimer) {
        case 0:
            unitMotSet(arg0->unit, 0x1F);
            if (!(*(int *)*(int *)((char *)arg0->unit + 0x10) & 0x40) && !(*(unsigned short *)((char *)calcUPGet((ObjectTask *)arg0->unit) + 0x1C) & 0x40)) {
                if ((calcUPGet((ObjectTask *)arg0->unit)->charaId < 0xBB) || (calcUPGet((ObjectTask *)arg0->unit)->charaId >= 0xC3)) {
                    decoEffCall((ObjectTask *)arg0->unit, 0);
                    unitFaceMotSet((ObjectTask *)arg0->unit, 0x36);
                    unitWpnMotSet(arg0->unit, dataDefWpnGet(arg0->unit), 0, 0x31);
                    if (calcUPGet((ObjectTask *)arg0->unit)->charaId == 7) {
                        unitWpnMotSet(arg0->unit, 1, 0, 0x31);
                    }
                    sndSysSePlay(0x1D);
                    sndSeTransPlay(arg0->unit, 0x17, ((int (*)(int *, int))etehrCVGet)((int *)arg0, arg1));
                    hairWindOn(arg0->unit);
                }
            }
            scenarioPtr.phaseTimer = 1;
            /* fallthrough */
        case 1:
            scenarioMtd(arg0, (int)ap);
            if (((int (*)(int *, int))scenarioBatDataSync)((int *)arg0, arg1) != 0 && (*(int *)((char *)*(int *)((char *)arg0->unit + 0x14) + 0x6F0) & 0x1000)) {
                scenarioPtr.phaseTimer = 2;
            } else {
                return;
            }
            /* fallthrough */
        default:
            unitFaceMotSet((ObjectTask *)arg0->unit, 0x35);
            hairWindOff(arg0->unit);
            scenarioPtrFuncSet((void (*)())scenarioBatExecPhase25);
            scenarioBatExecPhase25(arg0, arg1);
            return;
        }
    }
    if (state == 3) {
        switch (scenarioPtr.phaseTimer) {
        case 0:
            if (((int (*)(int *, int))scenarioBatDataSync)((int *)arg0, arg1) == 0) {
                return;
            }
            scenarioPtr.phaseTimer = 1;
            /* fallthrough */
        case 1:
            sndSeTransPlay(arg0->unit, 0x17, ((int)arg0->unit ^ *ap) == 0 ? 1 : 2);
            scenarioPtrFuncSet((void (*)())scenarioBatExecPhase25);
            scenarioBatExecPhase25(arg0, arg1);
            return;
        }
        return;
    }
    if (state == 1) {
        switch (scenarioPtr.phaseTimer) {
        case 0:
            temp_17 = *(short *)((char *)dataTecGet(*(int *)((char *)arg0 + 0x8)) + 0x14);
            temp_4_2 = *(signed char *)((char *)dataTecGet(*(int *)((char *)arg0 + 0x8)) + 0x11);
            if (temp_17 != 0) {
                if (temp_4_2 == 2) {
                    goto case2;
                } else if (temp_4_2 == 6) {
                    goto block_38;
                } else if (temp_4_2 == 5) {
                    goto block_38;
                } else {
                    goto skip;
                }
case2:
                unitMotSet(arg0->unit, 0x28);
                decoEffCall((ObjectTask *)arg0->unit, 1);
                sndSysSePlay(0x1E);
                sndSeTransPlay(arg0->unit, 0x17, 8);
block_38:
                dataUnitFileLoadMotSp(arg0->unit, (int)temp_17, 4);
                dataFileLoadWepSp((int)arg0->unit, *(int *)((char *)arg0 + 0x8));
skip:
                ;
            }
            scenarioPtr.phaseTimer = 1;
            /* fallthrough */
        case 1:
            if (((int (*)(int *, int))scenarioBatDataSync)((int *)arg0, arg1) != 0) {
                temp_4_2 = *(signed char *)((char *)dataTecGet(*(int *)((char *)arg0 + 0x8)) + 0x11);
                if (temp_4_2 == 2) {
                    if (!(*(int *)((char *)*(int *)((char *)arg0->unit + 0x14) + 0x6F0) & 0x1000)) {
                        return;
                    }
                }
                if (!(*(int *)*(int *)((char *)arg0->unit + 0x10) & 0x40) && !(*(unsigned short *)((char *)calcUPGet((ObjectTask *)arg0->unit) + 0x1C) & 0x40)) {
                    if ((calcUPGet((ObjectTask *)arg0->unit)->charaId < 0xBB) || (calcUPGet((ObjectTask *)arg0->unit)->charaId >= 0xC3)) {
                        temp_2_2 = sndConvSpid(arg0->unit, *(int *)((char *)arg0 + 0x4), *(int *)((char *)arg0 + 0x8), *(int *)((char *)arg0 + 0xC));
                        var_5 = 5;
                        if (temp_2_2 != 3) {
                            var_5 = (temp_2_2 ^ 4) ? 0 : 7;
                        }
                        if (var_5 != 0) {
                            dataSndSeLoad((int)arg0->unit, var_5);
                        }
                    }
                }
                scenarioPtrFuncSet((void (*)())scenarioBatExecPhase25);
                scenarioBatExecPhase25(arg0, arg1);
                return;
            }
            return;
        default:
            return;
        }
    }
    if (scenarioBatDataSync((int *)arg0) != 0) {
        scenarioPtrFuncSet((void (*)())scenarioBatExecPhase25);
        scenarioBatExecPhase25(arg0, arg1);
        return;
    }
    return;
}

extern int D_00A57C4C[];
void scenarioBatExecPhase30();
void mapDispOffAll(void);
void unitDispOffAll(void);
void mapDispOnAll(void);
void unitDispOnAll(void);
void fadeIn(int, int);
int mapNoGet(void);
void dataMapLoad(int);
extern int MMv2Play(const char *filename);
int MMv2IsPlaying(void);
void MMv2Stop(void);
extern char *mvFname[5];

void scenarioBatExecPhase25(void *arg0, int arg1) {
    int type = *(int *)((char *)arg0 + 0x8);
    if (type == 0x38 || type == 0x235 || type == 0x239) {
        switch (scenarioPtr.phaseTimer) {
        case 0:
            mapDispOffAll();
            unitDispOffAll();
            scenarioPtr.phaseTimer++;
            return;
        case 1: {
            int idx = 2;
            if (type != 0x38) {
                idx = (type == 0x235) ? 3 : 4;
            }
            MMv2Play(mvFname[idx]);
            scenarioPtr.phaseTimer++;
            return;
        }
        case 2:
            if (MMv2IsPlaying() == 0) {
                MMv2Stop();
                D_00A57C4C[0] = dataCdSync();
                dataMapLoad(mapNoGet());
                scenarioPtr.phaseTimer++;
            }
            return;
        case 3:
            {
                int cd = dataCdSync();
                if (D_00A57C4C[0] >= cd) {
                fadeIn(4, 0);
                mapDispOnAll();
                unitDispOnAll();
                scenarioPtr.phaseTimer++;
                }
            }
            return;
        case 4:
            scenarioPtrFuncSet(scenarioBatExecPhase30);
            scenarioBatExecPhase30(arg0, arg1);
            return;
        default:
            return;
        }
    }
phase30:
    scenarioPtrFuncSet(scenarioBatExecPhase30);
    scenarioBatExecPhase30(arg0, arg1);
}

extern char D_00A43F98[];
extern short *dataTecGet(int id);
void scenarioBatExecPhase40();
int scenarioEffCall(ScenarioBatExec *, u8 *);
void unitMotSetEx(void *, int, int);

void scenarioBatExecPhase30(ScenarioBatExec *arg0) {
    s16 temp_20;
    int temp_19;
    int var_18;
    int temp_16_2;
    int temp_2;
    int var_18_3;
    int var_7;
    int i;
    signed char temp_16;
    signed char var_16;
    u8 *temp_2_2;

    if (*(int *)((char *)arg0 + 0x4) == 2) {
        unitMotSet(arg0->unit, 0x20);
        (*(int *)((char *)arg0->unit->work + 0x70)) = 1;
    } else if (*(int *)((char *)arg0 + 0x4) == 3) {
        unitMotSet(arg0->unit, 0x27);
        unitWpnMotSet(arg0->unit, dataDefWpnGet(arg0->unit), 0, 0x33);
    } else if (*(int *)((char *)arg0 + 0x4) == 1) {
        temp_20 = *(s16 *)((char *)dataTecGet(*(int *)((char *)arg0 + 0x8)) + 0x14);
        temp_19 = *(signed char *)((char *)dataTecGet(*(int *)((char *)arg0 + 0x8)) + 0x11);
        if (temp_20 == 0) {
            var_18 = 1;
            temp_16 = *(signed char *)((char *)dataTecGet(*(int *)((char *)arg0 + 0x8)) + 0xB);
            unitMotSet(arg0->unit, (int) temp_16);
            if (*(int *)((char *)arg0 + 0x8) != 0x27) {
                var_18 = *(int *)((char *)arg0 + 0xC);
            }
            unitWpnMotSet(arg0->unit, var_18, 0, temp_16 + 0x22);
        } else {
            if (temp_19 == 2 || temp_19 == 6 || temp_19 == 5) {
                var_18 = 2;
                if (dataTid2WepSp(*(int *)((char *)arg0 + 0x8)) == 0) {
                    var_18 = *(int *)((char *)arg0 + 0xC);
                }
                dataUnitFileLoadMotSp2(arg0->unit, (int) temp_20, 4, var_18, (int) temp_19);
                dataFileLoadWepSp2(arg0->unit, *(int *)((char *)arg0 + 0x8));
                var_16 = *(signed char *)((char *)dataTecGet(*(int *)((char *)arg0 + 0x8)) + 0xB);
                unitMotSetEx(arg0->unit, (int) var_16, 4);
                if (temp_19 != 5) {
                    unitWpnMotSet(arg0->unit, var_18, 1, var_16 + 2);
                } else {
                    if (((signed char *)calcUPGet((ObjectTask *)arg0->unit))[0x5A] < 0x20) {
                        var_7 = 3;
                        var_18_3 = 4;
                    } else {
                        var_7 = 4;
                        var_18_3 = 3;
                    }
                    unitWpnMotSet(arg0->unit, 0, 1, var_7);
                    unitWpnMotSet(arg0->unit, 1, 1, var_18_3);
                }
            } else {
                var_16 = *(signed char *)((char *)dataTecGet(*(int *)((char *)arg0 + 0x8)) + 0xB);
                unitMotSetEx(arg0->unit, (int) var_16, *(int *)((char *)arg0 + 0xC) + 1);
                unitWpnMotSet(arg0->unit, *(int *)((char *)arg0 + 0xC), 0, var_16 + 2);
            }
            printf(D_00A43F98, temp_20, var_16, temp_19);
        }
        if (dataMtdRead(arg0->unit, 2) == 0) {
            spWepDispOn(arg0, dataDefWpnGet(arg0->unit));
        }
    }
    temp_16_2 = sndConvSpid(arg0->unit, *(int *)((char *)arg0 + 0x4), *(int *)((char *)arg0 + 0x8), *(int *)((char *)arg0 + 0xC));
    temp_2 = sndConvSe(arg0->unit, *(int *)((char *)arg0 + 0x4), *(int *)((char *)arg0 + 0x8));
    if (temp_16_2 > 0) {
        sndSeTransPlay((int) arg0->unit, temp_16_2, temp_2);
    }
    for (i = 0; i < *(int *)((char *)arg0 + 0x10); i++) {
        temp_2_2 = scenarioDstGet(i);
        scenarioEffCall(arg0, temp_2_2);
        (*(int *)((char *)temp_2_2 + 0x14)) = 0;
    }
    scenarioPtrFuncSet(scenarioBatExecPhase40);
}

typedef struct ScnExec50 {
    BattleUnit *unit;
    int kind;
    int tec;
    int itm;
} ScnExec50;

typedef struct {
    ObjectTask *unit;                     /* +0x00 */
    unsigned char unmodeled_04[0x8 - 0x4];
    int tecId;                            /* +0x08 */
} SyoukanRec;

void scenarioMtd(ScenarioBatExec *exec);
int calcHitMotChk(ObjectTask *unit);
int sefIsHitActor(void *actor);
int dataFileLoadWepSpEnd(ObjectTask *unit, int tid);
void unitWpnMotSet(ObjectTask *unit, int weaponIndex, int animGroup, int motion);
void unitCmdAtkEnd();
int scenarioSrcEndChk(int *exec, int unused, int dflt);
void scenarioBatExecMain();
int flowSpecProc(ScenarioBatExec *exec, void **arg1);
int flowSpecProcInit(ScenarioBatExec *exec, void **arg1);
void scenarioBatExecPhase50();

/* scenarioBatExecPhase40: scenario exec step; ends with the phase-50 handoff. */
void scenarioBatExecPhase40(ScenarioBatExec *arg0, void **arg1) {
    ScnExec50 *exec = (ScnExec50 *)arg0;
    int var;
    int off;
    int flags;

    if (scenarioPtr.phaseTimer < 2) {
      if (scenarioPtr.phaseTimer >= 0) {
        scenarioMtd(arg0);
        var = 0;
        if (exec->kind == 3 || !(dataTecGet(exec->tec)[1] & 0x20)) {
            scenarioBatExecMain((SyoukanRec *)arg0, (int)arg1);
        } else {
            if (scenarioPtr.phaseTimer == 0) {
                if ((*(int *)((char *)exec->unit->actor + 0x6F0) & 0x1000) || calcHitMotChk((ObjectTask *)exec->unit) != 0 ||
                    sefIsHitActor(*(void **)((char *)*arg1 + 0x14)) != 0) {
                    var = flowSpecProcInit(arg0, arg1);
                    scenarioPtr.phaseTimer = 1;
                }
            } else {
                var = flowSpecProc(arg0, arg1);
            }
        }
        if (scenarioSrcEndChk((int *)arg0, (int)arg1, var) == 0) {
            goto done;
        }
        {
            if (exec->kind == 1) {
                if (dataMtdRead(exec->unit, 3) == 0) {
                    spWepDispOff(arg0, dataDefWpnGet(exec->unit));
                }
                dataFileLoadWepSpEnd((ObjectTask *)exec->unit, exec->tec);
            }
            flags = calcUPGet((ObjectTask *)exec->unit)->flags;
            if (flags & 0x40) {
                int i = 0;
                off = 0x50;
                do {
                    if (*(short *)((char *)calcUPGet((ObjectTask *)exec->unit) + off + 0xE) != 0) {
                        unitWpnMotSet((ObjectTask *)exec->unit, i, 0, 5);
                    }
                    i++;
                    off += 2;
                } while (i < 3);
            } else {
                int i = 0;
                off = 0x50;
                do {
                    if (*(short *)((char *)calcUPGet((ObjectTask *)exec->unit) + off + 0xE) != 0) {
                        unitWpnMotSet((ObjectTask *)exec->unit, i, 0, 0xFF);
                    }
                    i++;
                    off += 2;
                } while (i < 3);
            }
            unitCmdAtkEnd((ObjectTask *)exec->unit, arg1[0], exec->kind, exec->tec);
            scenarioPtrFuncSet((ScenarioPhaseFunc)scenarioBatExecPhase50);
            scenarioBatExecPhase50(exec, arg1);
        }
      }
    }
done:
    return;
}

extern short *dataTecGet(int id);
extern short *dataItmGet(int id);
extern int sefIsFinishEffect(int id);
extern void statEffOn(ObjectTask *unit);

void scenarioBatExecPhase50(ScnExec50 *exec) {
    int id;
    CamEvent *ev;

    if (exec->kind != 3) {
        id = dataTecGet(exec->tec)[11];
    } else {
        id = dataItmGet(exec->itm)[8];
    }
    if (sefIsFinishEffect(id) != 0) {
        ev = &D_00A57B80;
        ev->type = 1;
        ev->unit = exec->unit;
        ev->param = exec->tec;
        ((void (*)(CamEvent *))camEventExec)(ev);
        statEffOn((ObjectTask *)exec->unit);
        scenarioBatNext();
    }
}

void scenarioBatExecMain(SyoukanRec *rec, int arg1, int arg2)
{
    int *rw = (int *)rec;
    int i;
    int flag;
    int hit;
    int *d;

    i = 0;
    if (rw[4] > 0) {
        do {
            flag = 0;
            d = (int *)scenarioDstGet(i);
            if (sefIsSeSignal(*(int *)(*d + 0x14)) != 0) {
                sndSePlay((int)rec->unit, sndConvSpid(rec->unit, rw[1], rw[2], rw[3]),
                          sndConvHitSe(rec->unit, rw[1], rw[2]) + d[5]);
            }
            hit = calcHitMotChk(rec->unit);
            if (hit != 0 || sefIsHitActor(*(int *)(*d + 0x14)) != 0) {
                if (d[1] != 0xA || (*(int *)(*(int *)(*d + 0x10)) & 0x4000) == 0) {
                    scenarioBatExecDst((int *)rec, d);
                    if (d[1] != 0xA) {
                        sefHitEffect();
                        if (hit != 0) {
                            sndSePlay((int)rec->unit, sndConvSpid(rec->unit, rw[1], rw[2], rw[3]),
                                      sndConvHitSe(rec->unit, rw[1], rw[2]) + d[5]);
                        }
                        flag = 1;
                    }
                    if (rw[1] != 3 && d[5] == *(signed char *)((char *)dataTecGet(rw[2]) + 0x10) - 1) {
                        goto dmg;
                    }
                    if (rw[1] == 3 && d[5] == 0) {
                        goto dmg;
                    }
                    if (d[1] == 0xA) {
dmg:
                        dmgValPut(rec, (u8 *)d);
                        if ((*(unsigned long long *)rec->unit->work & 0x2040) == 0x2000) {
                            menuCritDispOn(0);
                        }
                    }
                    if (flag != 0) {
                        d[5]++;
                    }
                    *(int *)(*(int *)(*d + 0x10)) |= 0x4000;
                }
            }
            i++;
        } while (i < rw[4]);
    }
}

/*
 * The stolen-item state nusumeSet/nusumeGet share (data still asm-owned;
 * original ELF symbol name).
 */
extern int nusumuFlag;

void nusumeSet(int value)
{
    nusumuFlag = value;
}

int nusumeGet(void)
{
    return nusumuFlag;
}

INCLUDE_ASM("asm/nonmatchings/ov01/battle_init", flowSpecProcInit);

INCLUDE_ASM("asm/nonmatchings/ov01/battle_init", flowSpecProc);

extern void unitCmdClear(ObjectTask *unit);
extern void unitCmdSet(ObjectTask *unit, int *cmd);

void syoukanFadeOut(BattleUnit *excludedUnit)
{
    int cmd[8];
    BattleUnit **unitTable;
    int count;
    int i;

    i = 0;
    count = unitTblGet(0, &unitTable);
    if (count > 0) {
        do {
            if (unitTable[i] != 0 && unitTable[i] != excludedUnit) {
                cmd[0] = 0x30;
                unitCmdClear((ObjectTask *)unitTable[i]);
                unitCmdSet((ObjectTask *)unitTable[i], cmd);
            }
            i++;
        } while (i < count);
    }
}

extern void unitCmdClear(ObjectTask *unit);
extern void unitCmdSet(ObjectTask *unit, int *cmd);

void syoukanFadeIn(BattleUnit *excludedUnit)
{
    int cmd[8];
    BattleUnit **unitTable;
    int count;
    int i;

    i = 0;
    count = unitTblGet(0, &unitTable);
    if (count > 0) {
        do {
            if (unitTable[i] != 0 && unitTable[i] != excludedUnit) {
                cmd[0] = 0x2F;
                unitCmdClear((ObjectTask *)unitTable[i]);
                unitCmdSet((ObjectTask *)unitTable[i], cmd);
            }
            i++;
        } while (i < count);
    }
}

void syoukan1Sub(SyoukanRec *rec, int arg1, int arg2)
{
    int cmd[8];

    unitCmdClear(rec->unit);
    cmd[0] = 0x2B;
    cmd[1] = 8;
    cmd[2] = arg2;
    unitCmdSet(rec->unit, cmd);
    if (*(unsigned char *)dataTecGet(rec->tecId) & 0x10) {
        cmd[0] = 0x2C;
    } else {
        cmd[0] = 0xF;
    }
    unitCmdSet(rec->unit, cmd);
    cmd[0] = 0x25;
    cmd[1] = 0;
    unitCmdSet(rec->unit, cmd);
    cmd[0] = 0xD;
    cmd[1] = 7;
    cmd[2] = 0;
    unitCmdSet(rec->unit, cmd);
}

extern void unitCmdClear(ObjectTask *unit);
extern void unitCmdSet(ObjectTask *unit, int *cmd);

void syoukan2Sub(ObjectTask **arg0) {
    int cmd[8];
    unitCmdClear(*arg0);
    cmd[0] = 0x2B;
    cmd[1] = *(short *)((char *)calcUPGet(*arg0)->linkedUnit + 0x44);
    cmd[2] = calcUPGet(*arg0)->linkedUnit->charaId;
    unitCmdSet(*arg0, cmd);
    cmd[0] = 0xD;
    cmd[1] = 1;
    cmd[2] = 1;
    unitCmdSet(*arg0, cmd);
    cmd[0] = 0x2F;
    unitCmdSet(*arg0, cmd);
}

extern int unitTelOut(ObjectTask *unit);
extern void scenarioBatExecMain(SyoukanRec *rec, int arg1, int arg2);
extern void statEffOff(ObjectTask *unit);

int specSyoukanProc(SyoukanRec *rec, int arg1, unsigned int *state, int arg3)
{
    int ret;
    unsigned int s;
    char *p;

    ret = 0;
    s = *state;
    switch (s) {
    case 0:
        syoukanFadeOut((BattleUnit *)rec->unit);
        p = *(char **)((char *)calcUPGet(rec->unit) + 0x158);
        if (p != 0) {
            *(int *)(p + 0xA8C) |= 0x100;
        }
        s++;
        statEffOff(rec->unit);
        break;
    case 1:
        if (unitTelOut(rec->unit) != 0) {
            s++;
        }
        break;
    case 2:
        s++;
        syoukan1Sub(rec, arg1, arg3);
        break;
    case 3:
        scenarioBatExecMain(rec, arg1, arg3);
        if (!(**(int **)((char *)rec->unit + 0x10) & 2)) {
            s++;
        }
        break;
    case 4:
        s++;
        syoukan2Sub(&rec->unit);
        break;
    case 5:
        if (!(**(int **)((char *)rec->unit + 0x10) & 2)) {
            s++;
            syoukanFadeIn((BattleUnit *)rec->unit);
        }
        break;
    case 6:
        p = *(char **)((char *)calcUPGet(rec->unit) + 0x158);
        if (p != 0) {
            *(int *)(p + 0xA8C) &= ~0x100;
        }
        ret = 1;
        statEffOn(rec->unit);
        break;
    }
    *state = s;
    return ret;
}

extern void dataLeaderCidReset(void);
extern void mapDispOffAll(void);
extern void unitDispOffAll(void);
extern void mapDispOnAll(void);
extern void unitDispOnAll(void);
extern void fadeIn(int a, int b);
extern int mapNoGet(void);
extern void dataMapLoad(int no);
extern int MMv2IsPlaying(void);
extern void MMv2Stop(void);
extern int MMv2Play(const char *filename);
extern int D_00A57C4C[];
extern char *mvFname[5];

int specHensinProc(void *arg0, int arg1, unsigned int *arg2)
{
    int cmd[8];
    int ret;
    unsigned int s;
    int v;

    s = *arg2;
    ret = 0;
    switch (s) {
    case 0:
        s++;
        /* fallthrough */
    case 1:
        if (unitTelOut(*(ObjectTask **)arg0) != 0) {
            s++;
        }
        break;
    case 2:
        unitCmdClear(*(ObjectTask **)arg0);
        dataLeaderCidReset();
        cmd[0] = 0x2B;
        if (*(short *)((char *)calcUPGet(*(ObjectTask **)arg0) + 0x44) < 0) {
            v = -*(short *)((char *)calcUPGet(*(ObjectTask **)arg0) + 0x44);
        } else {
            v = *(short *)((char *)calcUPGet(*(ObjectTask **)arg0) + 0x44);
        }
        s++;
        cmd[1] = v;
        cmd[2] = *(short *)((char *)calcUPGet(*(ObjectTask **)arg0) + 0x38);
        unitCmdSet(*(ObjectTask **)arg0, cmd);
        cmd[0] = 0x25;
        cmd[1] = 0;
        cmdListChange((int)*(ObjectTask **)arg0, (CmdListBody *)cmd);
        break;
    case 3:
        if (!(**(int **)((char *)*(ObjectTask **)arg0 + 0x10) & 2)) {
            if (calcUPGet(*(ObjectTask **)arg0)->charaId == 6) {
                s++;
                mapDispOffAll();
                unitDispOffAll();
            } else {
                ret = 1;
            }
        }
        break;
    case 4:
        s++;
        {
            unsigned int x = *(unsigned int *)((char *)arg0 + 0x8);
            unsigned int idx = x != 0x254;
            MMv2Play(mvFname[idx]);
        }
        break;
    case 5:
        if (MMv2IsPlaying() == 0) {
            s++;
            MMv2Stop();
            D_00A57C4C[0] = dataCdSync();
            dataMapLoad(mapNoGet());
        }
        break;
    case 6:
        v = dataCdSync();
        if (D_00A57C4C[0] >= v) {
            fadeIn(4, 0);
            s++;
            mapDispOnAll();
            unitDispOnAll();
        }
        break;
    case 7:
        ret = 1;
        break;
    }
    *arg2 = s;
    return ret;
}

extern void UmnDataBaseAnalisisSet(int monster_id);
extern const char **dataEnemyNameGet(int index);
extern void eBattleWinOpen3(void *param);
typedef struct {
    int width;
    int height;
    int color;
    const char *name;
    unsigned short x;
    unsigned short y;
    unsigned short page;
    int title;
    int text;
    int picture;
    int sound;
    unsigned long long model;
    unsigned short f30;
    unsigned short f32;
} SpecAnalyzeWin;

void specAnalyzeProc(ObjectTask *arg0)
{
    SpecAnalyzeWin w;
    unsigned char *temp;

    temp = dataGainGet(calcUPGet(arg0)->charaId);
    UmnDataBaseAnalisisSet(calcUPGet(arg0)->charaId);
    w.width = 0x10;
    w.height = 0x6C;
    w.color = 0xFFFFFD;
    w.name = *dataEnemyNameGet(calcUPGet(arg0)->charaId);
    w.x = calcUPGet(arg0)->currentHp;
    w.y = ((unsigned short *)calcUPGet(arg0))[0];
    w.page = ((unsigned short *)((char *)calcUPGet(arg0) + 0x18))[0];
    w.title = temp[0x6];
    w.text = temp[0x7];
    w.picture = temp[0x8];
    w.model = ((unsigned short *)temp)[2];
    w.sound = *(int *)temp;
    w.f30 = temp[0x10] + (temp[0xD] << 8);
    w.f32 = temp[0x11] + (temp[0xE] << 8);
    eBattleWinOpen3(&w);
}

void specEscapeProc(void)
{
    escapeFlag = 1;
}

typedef struct { signed char s[9]; } InoriAq;
typedef struct { int thr; int cnt; int *tab; } InoriEnt;
extern const char D_00A44010[];
extern const char D_00A44028[];
extern char *strcat(char *dst, const char *src);
extern InoriEnt inoriTbl[4];

void specInoriProc(void)
{
    int roll;
    int item;
    int i;

    roll = rnd(0x63);
    item = 0;
    for (i = 0; i < 4; i++) {
        if (roll < inoriTbl[i].thr) {
            item = inoriTbl[i].tab[rnd(inoriTbl[i].cnt - 1)];
            break;
        }
    }
    printf(D_00A44010, item);
    *(InoriAq *)msgWork.text = *(InoriAq *)D_00A44028;
    strcat(msgWork.text, *dataItmNameGet(item));
    strcat(msgWork.text, D_00A43ED0);
    battleMsg((int)&msgWork);
    dataBoxInc(0, item);
}

typedef struct { signed char s[7]; } NusumuStole;
typedef struct { signed char s[25]; } NusumuNone;
extern const char *itemNameGet(int group, int index);
extern int calcStatGet(BattleUnit *unit, int stat, int mask);
extern int calcStatSet(BattleUnit *unit, int stat, int mask, int value);
extern char *strcat(char *dst, const char *src);

int specNusumuProc(void *arg0, ObjectTask **arg1)
{
    unsigned char *temp;
    int check;
    unsigned char grp;
    unsigned char id;
    const char *name;

    temp = dataGainGet(calcUPGet(*arg1)->charaId);
    check = 1;
    if (*(int *)((char *)arg0 + 0x8) != 0x5A) {
        check = rnd(0x64) >= 0x33;
    }
    if (check != 0 && temp[0x11] != 0 && calcStatGet((BattleUnit *)*arg1, 7, 8) == 0) {
        grp = temp[0xE];
        id = temp[0x11];
        calcStatSet((BattleUnit *)*arg1, 7, 8, 0);
    } else if (temp[0x10] != 0 && calcStatGet((BattleUnit *)*arg1, 7, 0x10) == 0) {
        grp = temp[0xD];
        id = temp[0x10];
        calcStatSet((BattleUnit *)*arg1, 7, 0x10, 0);
    } else {
        grp = 0;
        id = 0;
    }
    if (itemGrpChk(grp, id) == 0) {
        grp = 0;
        id = 0;
    }
    printf("** Nusumu grp=%d item=%d\n", grp, id);
    if (id != 0) {
        name = itemNameGet(grp, id);
        if (name != 0) {
            *(NusumuStole *)msgWork.text = *(NusumuStole *)"Stole ";
            strcat((char *)msgWork.text, name);
            strcat((char *)msgWork.text, D_00A43ED0);
            dataBoxInc(grp, id);
            return 1;
        }
        return (int)name;
    }
    *(NusumuNone *)msgWork.text = *(NusumuNone *)"Couldn't steal anything!";
    return 0;
}

const char *itemNameGet(int group, int index)
{
    const char *name;

    name = 0;
    switch (group) {
    case 0:
        name = *dataItmNameGet(index);
        break;
    case 1:
        name = *dataWepNameGet(index);
        break;
    case 2:
        name = *dataBltNameGet(index);
        break;
    case 3:
        name = *dataAccNameGet(index);
        break;
    default:
        printf(D_00A44080, group, index);
        break;
    }
    return name;
}

void battleMsg(int msgId)
{
    battleMsgPut(0x10, 0x15C, msgId, 0x1E);
}

extern const char D_00A44098[];

int scenarioBatExecDst(int *arg0, int *arg1)
{
    int cmd[8];
    int self;
    int type;

    type = arg1[1];
    cmd[1] = arg0[2];
    self = arg0[0];
    cmd[3] = arg0[3];
    cmd[2] = self;
    switch (type) {
    case 9:
        cmd[0] = 9;
        cmd[1] = arg1[4];
        break;
    case 11:
        cmd[0] = 11;
        break;
    case 10:
        cmd[0] = 10;
        break;
    case 39:
        cmd[0] = 39;
        if (self == arg1[0]) {
            cmd[1] = 1;
        } else {
            cmd[1] = 0;
        }
        break;
    case 40:
        cmd[0] = 40;
        break;
    case 41:
        cmd[0] = 41;
        if (self == arg1[0]) {
            cmd[1] = 1;
        } else {
            cmd[1] = 0;
        }
        break;
    default:
        printf(D_00A44098, type);
        return 0;
    }
    unitCmdClear((ObjectTask *)arg1[0]);
    unitCmdSet((ObjectTask *)arg1[0], cmd);
    return 1;
}

typedef struct DecoEffEntry {
    int charaId;
    int subId[2];
} DecoEffEntry;
extern DecoEffEntry decoEff[];
void decoEffCallSub(BattleUnit *unit, int subId);

void decoEffCall(ObjectTask *unit, int type) {
    short charaId;
    int subId;
    int i;

    subId = 0;
    charaId = calcUPGet(unit)->charaId;
    if (type == 0x64) {
        switch (((signed char *)calcUPGet(unit))[0x15]) {
        case 0:
        case 1:
            subId = 0xA3D;
            break;
        case 2:
            subId = 0xA3E;
            break;
        case 3:
        case 4:
            subId = 0xA3F;
            break;
        }
    } else if (type == 0x65) {
        subId = 0xA28;
    } else if (type == 0x66) {
        subId = 0xA40;
    } else if (type == 0x67) {
        subId = 0xA41;
    } else if (type == 0x68) {
        subId = 0x4000;
    } else {
        i = 0;
        while (decoEff[i].charaId != 0) {
            if (charaId == decoEff[i].charaId) {
                subId = decoEff[i].subId[type];
                break;
            }
            i++;
        }
    }
    if (subId != 0) {
        decoEffCallSub((BattleUnit *)unit, subId);
    }
}

/*
 * decoEffCallSub's own signed byte inside calc.h's unmodeled_10 span (that
 * header is owned by ov01/tu004 and not completed here); shared-header need.
 */
#define CALC_PARAM_DECO_EFF_OFF 0x15

/*
 * calc.h's own charaId (+0x38) is signed; decoEffCallSub reads it here with
 * an unsigned halfword load (lhu, not the lh every other charaId read in
 * this TU uses), so it is read through this raw offset instead of the
 * shared ->charaId member.
 */
#define CALC_PARAM_CHARA_ID_OFF 0x38

extern void sefCreateEffect(int *params);

/*
 * The local effect-request block sefCreateEffect reads (evidenced only by
 * this call: only the fields decoEffCallSub itself stores are named).
 */
typedef struct DecoEffParams {
    Actor *actor;              /* +0x00 */
    unsigned char unmodeled_04[0xC - 0x4];
    short moveVariant;         /* +0x0C */
    unsigned char unmodeled_0e[0x10 - 0xE];
    short charaId;             /* +0x10 */
    unsigned char unmodeled_12[0x14 - 0x12];
    short subId;               /* +0x14 */
    unsigned char unmodeled_16[0x1A - 0x16];
    short flags;               /* +0x1A */
    unsigned char unmodeled_1c[0x24 - 0x1C];
} DecoEffParams;

void decoEffCallSub(BattleUnit *unit, int subId) {
    DecoEffParams params;
    unsigned short charaId;

    memset(&params, 0, sizeof(params));
    params.actor = unit->actor;
    params.moveVariant = (signed char)((signed char *)calcUPGet((ObjectTask *)unit))[CALC_PARAM_DECO_EFF_OFF];
    charaId = *(unsigned short *)((unsigned char *)calcUPGet((ObjectTask *)unit) + CALC_PARAM_CHARA_ID_OFF);
    params.subId = (short)subId;
    params.charaId = charaId;
    params.flags = (short)calcUPGet((ObjectTask *)unit)->flags;
    sefCreateEffect((int *)&params);
}

extern int calcAtrChg(ObjectTask *unit, int attribute);
extern int dataTid2WepSp(int id);
extern int calcStatGet(BattleUnit *unit, int stat, int mask);
typedef struct ScnEffArg {
    BattleUnit *unit;  /* +0x00 */
    int kind;          /* +0x04: 3 = item, otherwise technique */
    int tec;           /* +0x08 */
    int slot;          /* +0x0C: item id when kind == 3, else weapon slot */
} ScnEffArg;
typedef struct ScnEffTgt {
    BattleUnit *unit;            /* +0x00 */
    unsigned char unmodeled_04[0x0C - 0x04];
    unsigned short slot;         /* +0x0C */
} ScnEffTgt;
typedef struct ScnEffParams {
    Actor *actor0;               /* +0x00 */
    Actor *actor1;               /* +0x04 */
    Actor *weapon;               /* +0x08 */
    short var0c;                 /* +0x0C */
    short var0e;                 /* +0x0E */
    unsigned short cid0;         /* +0x10 */
    unsigned short cid1;         /* +0x12 */
    short spid;                  /* +0x14 */
    unsigned short slot16;       /* +0x16 */
    short atr;                   /* +0x18 */
    unsigned short flags1;       /* +0x1A */
    short cfev;                  /* +0x1C */
    short stat;                  /* +0x1E */
    short var20;                 /* +0x20 */
} ScnEffParams;

int scenarioEffCall(ScenarioBatExec *arg0p, u8 *arg1p)
{
  ScnEffArg *arg0 = (ScnEffArg *)arg0p;
  ScnEffTgt *arg1 = (ScnEffTgt *)arg1p;
  Actor *new_var;
  ScnEffParams p;
  int idx;
  int sl;
  short spid;
  signed char cat;
  if (arg0->kind == 3)
  {
    cat = 0;
    spid = dataItmGet(arg0->slot)[8];
    p.weapon = 0;
    p.atr = 0;
  }
  else
  {
    short *tec;
    tec = dataTecGet(arg0->tec);
    spid = tec[11];
    tec = dataTecGet(arg0->tec);
    do
    {
    }
    while (0);
    p.atr = calcAtrChg((ObjectTask *) arg0->unit, *((unsigned short *) (tec + 9)));
    cat = ((signed char *) dataTecGet(arg0->tec))[0x11];
    if (arg0->slot >= 0)
    {
      idx = 0;
      if (cat != 5)
      {
        if (0 != dataTid2WepSp(arg0->tec))
        {
          idx = 2;
        }
        else
        {
          idx = arg0->slot;
        }
      }
      p.weapon = arg0->unit->weapon[idx & 3];
    }
    else
    {
      p.weapon = 0;
    }
  }
  new_var = arg0->unit->actor;
  p.actor1 = arg1->unit->actor;
  p.actor0 = new_var;
  p.var0c = ((signed char *) calcUPGet((ObjectTask *) arg0->unit))[0x15];
  p.var0e = ((signed char *) calcUPGet((ObjectTask *) arg1->unit))[0x15];
  p.cid0 = *((unsigned short *) (((char *) calcUPGet((ObjectTask *) arg0->unit)) + 0x38));
  p.cid1 = *((unsigned short *) (((char *) calcUPGet((ObjectTask *) arg1->unit)) + 0x38));
  p.slot16 = arg1->slot;
  p.spid = spid;
  p.flags1 = calcUPGet((ObjectTask *) arg1->unit)->flags;
  p.cfev = 0;
  {
    int upi = (int) calcUPGet((ObjectTask *) arg0->unit);
    idx = arg0->slot;
    idx += upi;
    p.var20 = (signed char) *(unsigned char *)(idx + 0x5A);
  }
  p.cfev = cfEventGet();
  if (calcStatGet(arg1->unit, 1, 0x100) != 0)
  {
    p.stat = 1;
  }
  else
  {
    p.stat = 0;
  }
  sefCreateEffect((int *) (&p));
  if (cat == 5)
  {
    p.weapon = arg0->unit->weapon[1];
    p.var20 = ((signed char *) calcUPGet((ObjectTask *) arg0->unit))[0x5B];
    p.spid = spid + 100;
    sefCreateEffect((int *) (&p));
  }
}

void scenarioMtd(ScenarioBatExec *exec)
{
    if (dataMtdRead(exec->unit, 2) == 1) {
        spWepDispOn(exec, dataDefWpnGet(exec->unit));
    }
    if (dataMtdRead(exec->unit, 3) == 1) {
        spWepDispOff(exec, dataDefWpnGet(exec->unit));
    }
    if (dataMtdRead(exec->unit, 4) == 1) {
        ACT_setHand(exec->unit->actor, 0);
    }
    if (dataMtdRead(exec->unit, 5) == 1) {
        ACT_setHand(exec->unit->actor, 1);
    }
    if (dataMtdRead(exec->unit, 6) == 1) {
        spWepDispOn(exec, 1);
    }
    if (dataMtdRead(exec->unit, 7) == 1) {
        spWepDispOff(exec, 1);
    }
    if (dataMtdRead(exec->unit, 8) == 1) {
        hairWindOn(exec->unit);
    }
    if (dataMtdRead(exec->unit, 9) == 1) {
        hairWindOff(exec->unit);
    }
}

extern int dataTid2WepSp(int id);
extern void ACT_setArms(Actor *arms, Actor *actor, int a, int b);
void spWepDispOnSub(BattleUnit *unit, int slot, int fade);

void spWepDispOn(ScenarioBatExec *exec, int weapon)
{
    int *e = (int *)exec;
    BattleUnit *unit;
    Actor *wep;
    int kind;
    int t;
    char *up;

    if (e[1] == 1) {
        t = ((signed char *)dataTecGet(e[2]))[0x11];
    } else {
        spWepDispOnSub(exec->unit, weapon, 0);
        return;
    }
    if (t == 6) {
        spWepDispOnSub(exec->unit, e[3], 1);
    } else if (dataTid2WepSp(e[2]) != 0) {
        kind = calcUPGet((ObjectTask *)exec->unit)->sefSetupParams[2];
        if ((unsigned int)(kind - 13) >= 4) {
            wep = exec->unit->weapon[weapon & 3];
            if (wep != 0) {
                *(int *)wep |= WEAPON_HIDDEN_BIT;
            }
        } else {
            unit = exec->unit;
            ACT_setArms(unit->weapon[2], unit->actor, 0x108, 0);
        }
        spWepDispOnSub(exec->unit, 2, 0);
    } else if (e[2] == 8 || e[2] == 11) {
        up = (char *)calcUPGet((ObjectTask *)exec->unit);
        kind = *(short *)(up + e[3] * 2 + 0x5E);
        if (kind == 12) {
            spWepDispOnSub(exec->unit, e[3], 0);
        }
    }
}

/*
 * spWepDispOn's worker: with fade clear the weapon actor in slot `slot & 3`
 * is revealed at once, otherwise transWepIn fades it in.
 */
void spWepDispOnSub(BattleUnit *unit, int slot, int fade)
{
    if (fade == 0) {
        unit->weapon[slot & 3]->flags &= ~WEAPON_HIDDEN_BIT;
        return;
    }
    transWepIn(unit, slot);
}

void spWepDispOffSub(BattleUnit *unit, int slot, int fade);

void spWepDispOff(ScenarioBatExec *exec, int weapon)
{
    int *e = (int *)exec;
    Actor *wep;
    int kind;
    int t;
    char *up;

    if (e[1] == 1) {
        t = ((signed char *)dataTecGet(e[2]))[0x11];
    } else {
        spWepDispOffSub(exec->unit, weapon, 0);
        return;
    }
    kind = calcUPGet((ObjectTask *)exec->unit)->sefSetupParams[2];
    if (t == 6) {
        spWepDispOffSub(exec->unit, e[3], 1);
    } else if (kind == 17 || kind == 11) {
        spWepDispOffSub(exec->unit, 2, 1);
    } else if (dataTid2WepSp(e[2]) != 0) {
        kind = calcUPGet((ObjectTask *)exec->unit)->sefSetupParams[2];
        if ((unsigned int)(kind - 13) >= 4) {
            wep = exec->unit->weapon[weapon & 3];
            if (wep != 0 && kind != 17) {
                *(int *)wep &= ~WEAPON_HIDDEN_BIT;
            }
        } else {
            ACT_setHand(exec->unit->actor, 0);
        }
        spWepDispOffSub(exec->unit, 2, 0);
    } else if (e[2] == 8 || e[2] == 11) {
        up = (char *)calcUPGet((ObjectTask *)exec->unit);
        kind = *(short *)(up + e[3] * 2 + 0x5E);
        if (kind == 12) {
            spWepDispOffSub(exec->unit, e[3], 0);
        }
    }
}

/*
 * spWepDispOff's worker: with fade clear the weapon actor in slot
 * `slot & 3` is hidden at once, otherwise transWepOut fades it out.
 */
void spWepDispOffSub(BattleUnit *unit, int slot, int fade)
{
    if (fade == 0) {
        unit->weapon[slot & 3]->flags |= WEAPON_HIDDEN_BIT;
        return;
    }
    transWepOut(unit, slot);
}

void transWepIn(BattleUnit *unit, int slot)
{
    Actor *weapon;

    weapon = unit->weapon[slot & 3];
    if (weapon != 0) {
        weapon->flags &= ~WEAPON_HIDDEN_BIT;
        weapon->draw = transWepInDraw;
        weapon->transparency = 0.0f;
    }
}

extern void unitActdraw(void);

void transWepInDraw(Actor *actor) {
    float transparency;

    unitActdraw();
    transparency = actor->transparency + 0.1f;
    actor->transparency = transparency;
    if (transparency >= 1.0f) {
        actor->transparency = 1.0f;
        actor->draw = 0;
    }
}

void transWepOut(BattleUnit *unit, int slot)
{
    Actor *weapon;

    weapon = unit->weapon[slot & 3];
    if (weapon != 0) {
        weapon->draw = transWepOutDraw;
        weapon->transparency = 1.0f;
    }
}

void transWepOutDraw(Actor *actor) {
    float transparency;

    unitActdraw();
    transparency = actor->transparency - 0.1f;
    actor->transparency = transparency;
    if (transparency <= 0.0f) {
        actor->transparency = 0.0f;
        actor->draw = 0;
        actor->flags |= WEAPON_HIDDEN_BIT;
    }
}

extern void unitMotStandSet(void *unit);

int scenarioSrcEndChk(int *exec, int unused, int dflt)
{
    short id;
    int *unit;
    int ret;

    if (exec[1] != 3 && ((unsigned short)dataTecGet(exec[2])[1] & 0x20)) {
        return dflt;
    }
    if ((unsigned int)(exec[1] - 2) < 2) {
        if (exec[1] != 3) {
            id = dataTecGet(exec[2])[11];
        } else {
            id = dataItmGet(exec[3])[8];
            unit = (int *)exec[0];
            if (*(int *)(unit[5] + 0x6F0) & 0x1000) {
                unitMotStandSet(unit);
            }
        }
        ret = sefIsFinishEffect(id);
    } else {
        ret = *(int *)(((int *)exec[0])[5] + 0x6F0);
        ret &= 0x1000;
    }
    return ret != 0;
}

extern int D_00A57B5C;
extern char D_00A44168[];
extern int calcIdol2Chk(ObjectTask *unit);
extern int calcUseItem(ObjectTask *a, ObjectTask *b, int c);
extern int calcHpDmg(void *a, ObjectTask *b, int c, int d);
extern int calcWct(ObjectTask *unit);

void scenarioBatEnd(void)
{
    int *src;
    int *dst;
    int i;
    int idolBefore;
    int idolAfter;
    int dmg;
    int stat;
    ObjectTask *unit;
    ObjectTask *unitB;
    CalcUnitParam *up;
    CalcUnitParam *up2;
    int wct;

    D_00A57B5C = 0;
    if (scenarioDstGet(0) != 0) {
        i = 0;
        src = (int *)((int (*)(int))scenarioSrcGet)(0);
        if (src[4] > 0) {
            do {
                dst = (int *)scenarioDstGet(i);
                unit = (ObjectTask *)dst[0];
                *(int *)unit->work = *(int *)unit->work & ~0x4000;
                idolBefore = calcIdol2Chk(unit);
                unitB = (ObjectTask *)dst[0];
                if (unitB == 0 || (*(int *)unitB->work & 0x80)) {
                    idolBefore = 0;
                }
                if (src[1] == 3) {
                    dmg = calcUseItem((ObjectTask *)src[0], unitB, src[3]);
                } else {
                    dmg = calcHpDmg((void *)src[0], unitB, dst[3], dst[2]);
                }
                if (src[2] == 0x24C) {
                    up = calcUPGet((ObjectTask *)dst[0]);
                    up2 = calcUPGet((ObjectTask *)dst[0]);
                    wct = calcWct((ObjectTask *)dst[0]);
                    up2->waitCount = wct;
                    *(short *)((char *)up + 0x42) = (short)wct;
                    calcUPGet((ObjectTask *)dst[0])->turnBias = 0;
                }
                if (src[2] == 0x24D) {
                    calcStatResetAll((ObjectTask *)dst[0]);
                }
                if (calcStatGet((ObjectTask *)dst[0], 2, 1) != 0 && dmg == 0) {
                    calcHp((ObjectTask *)dst[0], 1);
                    calcStatReset((ObjectTask *)dst[0], 2, 1, 0);
                    printf(D_00A44168);
                }
                stat = dst[6];
                if (stat != 8) {
                    calcStatSet((ObjectTask *)dst[0], stat, dst[7], 0);
                }
                stat = dst[8];
                if (stat == 8) {
                    if (dst[9] != 0) {
                        calcStatResetCha((ObjectTask *)dst[0]);
                    }
                } else {
                    calcStatReset((ObjectTask *)dst[0], stat, dst[9], 0);
                }
                idolAfter = calcIdol2Chk((ObjectTask *)dst[0]);
                if (idolBefore == 0) {
                    if (idolAfter == 0) {
                    } else {
                        goto block_30;
                    }
                } else if (idolAfter == 0) {
block_30:
                    unitMotStandSet((ObjectTask *)dst[0]);
                }
                if (*(int *)((ObjectTask *)dst[0])->work & 0x20) {
                    thinkUnitDmgExec((ObjectTask *)dst[0], (BattleUnit *)src);
                }
                if (dmg != 0 && src[1] == 1) {
                    counterProc(src, (BattleUnit **)dst);
                }
                i++;
            } while (i < src[4]);
        }
    }
}

extern int pCounterUnit;
extern char D_00A44178[];
extern int calcCounter(BattleUnit *unit, int tec);
extern int calcStatGet(BattleUnit *unit, int stat, int mask);
extern void calcBp(BattleUnit *unit, int bp);
extern int unitNoGet(BattleUnit *unit);

int counterProc(int *arg0, BattleUnit **arg1)
{
    BattleUnit *unit;
    int hit;

    hit = 0;
    unit = *arg1;
    if (!(*(unsigned char *)dataTecGet(arg0[2]) & 0x80)) {
        return 0;
    }
    if (!(*(int *)unit->work & 0x40)) {
        if (calcCounter(unit, arg0[2]) != 0) {
            if (calcStatGet(unit, 7, 0x800) != 0 && calcBoostChk(unit, 0) != 0) {
                goto boost;
            }
            calcBp(unit, 0x19);
            hit = 1;
        }
    } else if (calcCounter(unit, arg0[2]) != 0 && calcBoostChk(unit, 0) != 0) {
boost:
        calcBoost(unit, 0);
        hit = 1;
    }
    if (hit == 1) {
        pCounterUnit = arg0[0];
        printf(D_00A44178, unitNoGet(unit));
    } else {
        pCounterUnit = 0;
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ov01/battle_init", dmgValPut);

/*
 * Battle monster-gain tallies (original ELF symbol name; only its total size is
 * evidenced, by monsGainInit's own memset call).
 */
typedef struct MonsGain {
    unsigned char unmodeled_00[0x58];
} MonsGain;
extern MonsGain monsGain;

MonsGain *monsGainPtrGet(void)
{
    return &monsGain;
}

void monsGainInit(void)
{
    memset(&monsGain, 0, sizeof(MonsGain));
}

extern int calcSkill(ObjectTask *unit, int a, int b);
extern const char D_00A442A0[];

void monsGainSet(int unit)
{
  ObjectTask *arg0 = (ObjectTask *) unit;
  CalcUnitParam *up;
  unsigned char *rec;
  unsigned char *mg;
  void *p150;
  int sk_val;
  int x;
  int var_3;
  int grp;
  int id;
  int var_16;
  int t4;
  int cnt;
  int *cp;
  unsigned char *g;
  unsigned char *ar;
  up = calcUPGet(arg0);
  mg = (char *) up;
  p150 = *((void **) (mg + 0x150));
  if (p150 != 0)
  {
    grp = 0;
    mg = (unsigned char *) (&monsGain);
    rec = (unsigned char *) dataGainGet(up->charaId);
    id = 0;
    *((int *) (mg + 0x10)) = (*((int *) (mg + 0x10))) + (*((unsigned short *) (rec + 4)));
    *((int *) (mg + 0x0)) = (*((int *) (mg + 0x0))) + (*((int *) rec));
    if (menuTimeGet() == 3)
    {
      x = menuTimeXGet();
      *((int *) (mg + 0x4)) = (*((int *) (mg + 0x4))) + (rec[6] * x);
      *((int *) (mg + 0x8)) = (*((int *) (mg + 0x8))) + (rec[7] * x);
      t4 = (*((int *) (mg + 0xC))) + (rec[8] * x);
      *((int *) (mg + 0xC)) = t4;
      printf(D_00A442A0, x, *((int *) (mg + 0x4)), *((int *) (mg + 0x8)), t4);
    }
    else
    {
      *((int *) (mg + 0x4)) = (*((int *) (mg + 0x4))) + rec[6];
      *((int *) (mg + 0x8)) = (*((int *) (mg + 0x8))) + rec[7];
      *((int *) (mg + 0xC)) = (*((int *) (mg + 0xC))) + rec[8];
    }
    var_16 = rec[0xB], sk_val = calcSkill(p150, 0x1000, 0x8000);
    var_3 = var_16 < 0x65;
    if (sk_val > 0)
    {
      var_16 += (var_16 * sk_val) / 100;
      ;
    }
    if ((var_16 < 0x65) == 0)
    {
      var_16 = 0x64;
    }
    if ((rnd(0x64) < var_16) && (calcStatGet((BattleUnit *) arg0, 7, 8) == 0))
    {
      grp = rec[0xE];
      id = rec[0x11];
    }
    else
      if ((rnd(0x64) < rec[0xA]) && (calcStatGet((BattleUnit *) arg0, 7, 0x10) == 0))
    {
      grp = rec[0xD];
      id = rec[0x10];
    }
    if ((id != 0) && (itemGrpChk(grp, id) != 0))
    {
      g = (unsigned char *) (&monsGain);
      ar = g + 4;
      *((int *) ((ar + ((*((int *) (g + 0x54))) * 4)) + 0x10)) = grp;
      cp = (int *) (g + 0x54);
      cnt = *cp;
      *((int *) ((ar + (cnt * 4)) + 0x30)) = id;
      *cp = cnt + 1;
    }
  }
}

int itemGrpChk(int grp, int id)
{
    switch (grp) {
    case 0: if (id >= 0x49) return 0; break;
    case 1: if (id >= 0x74) return 0; break;
    case 2: if (id >= 0xB3) return 0; break;
    case 3: if (id >= 0xB5) return 0; break;
    default: return 0;
    }
    return 1;
}

typedef struct {
    int w0;
    void *actor;
    int w2;
    short s0c;
    short s0e;
    unsigned char p10[2];
    unsigned short u12;
    unsigned short u14;
    unsigned char tail[0x24 - 0x16];
} StatEffArg;
typedef struct {
    unsigned short id;
    unsigned short pad;
} StatEffIdEnt;
typedef struct {
    unsigned char pad0[0x6F0];
    volatile int flags6F0;
    float elapsed6F4;
    float rate6F8;
} StatEffActor;
extern StatEffIdEnt effId[8];
extern signed char effTbl[128];
extern void *sefCreateEffect2(void *arg);
extern void sefDeleteEffect2(void *p);
extern void unitFaceMotSet(BattleUnit *unit, int motion);
extern int calcStatIdx2Bit(int idx);

void statEffProc(BattleUnit *arg0)
{
    StatEffArg sp;
    int cnt[8];
    int i;
    int j;
    int off;
    int c3;
    int hit;
    void *eff;
    CalcUnitParam *up;

    memset(cnt, 0, 0x20);
    memset(&sp, 0, 0x24);
    for (i = 0; i < 8; i++) {
        signed char *p = effTbl + i * 16;
        int k = 0;
        do {
            hit = calcStatGet(arg0, i, calcStatIdx2Bit(k));
            k++;
            if (hit != 0 && *p != -1) {
                cnt[*p] = cnt[*p] + 1;
            }
            p++;
        } while (k < 15);
    }
    sp.actor = arg0->actor;
    j = 0;
    c3 = 3;
    sp.u12 = *(unsigned short *)((char *)calcUPGet((ObjectTask *)arg0) + 0x38);
    sp.s0e = ((signed char *)calcUPGet((ObjectTask *)arg0))[0x15];
    off = 0x150;
    for (; j < 8; j++) {
        if (cnt[j] != 0) {
            if (*(void **)((char *)calcUPGet((ObjectTask *)arg0) + off + 0xC) == 0) {
                sp.u14 = effId[j].id;
                up = calcUPGet((ObjectTask *)arg0);
                eff = sefCreateEffect2(&sp);
                *(void **)((char *)up + off + 0xC) = eff;
                if (j == 0) {
                    unitFaceMotSet(arg0, 0x37);
                }
                if (j == c3) {
                    ((StatEffActor *)arg0->actor)->flags6F0 |= 0x80000;
                    ((StatEffActor *)*(void * volatile *)&arg0->actor)->rate6F8 = 0;
                }
            }
        } else if (*(void **)((char *)calcUPGet((ObjectTask *)arg0) + off + 0xC) != 0) {
            sefDeleteEffect2(*(void **)((char *)calcUPGet((ObjectTask *)arg0) + off + 0xC));
            *(void **)((char *)calcUPGet((ObjectTask *)arg0) + off + 0xC) = 0;
            if (j == 0) {
                unitFaceMotSet(arg0, 0x35);
            }
            if (j == c3) {
                ((StatEffActor *)arg0->actor)->flags6F0 &= 0xFFF7FFFF;
                ((StatEffActor *)*(void * volatile *)&arg0->actor)->rate6F8 = 1.0f / 30.0f;
            }
        }
        off += 4;
    }
}

/*
 * calcUPGet's return type comes from its definer, ov01/tu004 calc.c
 * (published in include/ov01/calc.h); calcUPGet itself is still asm there.
 */
extern CalcUnitParam *calcUPGet(ObjectTask *unit);

#define UNIT_WORK_TABLE_COUNT 8
#define UNIT_WORK_STATUS_BLOCKED 0x100u
/* CalcUnitParam's own UnitWork* table base and per-slot pointer offset
 * (see the comment below); named to match this file's own CURSOR_STATE_OFF
 * convention instead of leaving the offsets as bare literals. */
#define UNIT_WORK_TABLE_BASE_OFF 0x150
#define UNIT_WORK_PTR_OFF 0xC

/*
 * Each battle unit's own large parameter block, reached only through the
 * per-unit table below; nothing else in this TU dereferences it.
 */
typedef struct UnitWork {
    unsigned char unmodeled_00[0xA8C];
    unsigned int flags; /* +0xA8C: UNIT_WORK_STATUS_BLOCKED suppresses
                          * status-effect ticks for this unit */
} UnitWork;

/*
 * calcUPGet's own CalcUnitParam (ov01/tu004 calc.c, still asm there, not
 * yet published to add this member -- reported as a shared-header need)
 * carries an 8-entry UnitWork* table at +0x15C. statEffOn/statEffOff each
 * re-read calcUPGet(unit)'s table slot for the current offset twice (once
 * to test it, once to use it) rather than caching it in a local: the
 * original calls calcUPGet a second time inside the body instead of
 * keeping the first call's result. The offset walks the table forward
 * (+4 each pass) while a separate trip counter counts the 8 passes down.
 */
void statEffOn(ObjectTask *unit)
{
    int offset;
    int i;

    offset = UNIT_WORK_TABLE_BASE_OFF;
    for (i = UNIT_WORK_TABLE_COUNT - 1; i >= 0; i--) {
        if (*(UnitWork **)((char *)calcUPGet(unit) + offset + UNIT_WORK_PTR_OFF) != 0) {
            (*(UnitWork **)((char *)calcUPGet(unit) + offset + UNIT_WORK_PTR_OFF))->flags
                &= ~UNIT_WORK_STATUS_BLOCKED;
        }
        offset += 4;
    }
}

void statEffOff(ObjectTask *unit)
{
    int offset;
    int i;

    offset = UNIT_WORK_TABLE_BASE_OFF;
    for (i = UNIT_WORK_TABLE_COUNT - 1; i >= 0; i--) {
        if (*(UnitWork **)((char *)calcUPGet(unit) + offset + UNIT_WORK_PTR_OFF) != 0) {
            (*(UnitWork **)((char *)calcUPGet(unit) + offset + UNIT_WORK_PTR_OFF))->flags
                |= UNIT_WORK_STATUS_BLOCKED;
        }
        offset += 4;
    }
}

/*
 * objEntry2 and objStdInit are defined by ov01/tu001 obj.c (src/ov01/obj.h
 * is that TU-local header and is not includable here);
 * src/ov01/entry_first_init.c already redeclares them the same way.
 */
extern ObjectTask *objEntry2(void *argument, void (*callback)(ObjectTask *task));
extern void objStdInit(ObjectTask *task);

/*
 * The enemy cursor's own task callbacks, both defined further down in this
 * TU: objCur (still asm) reaches the cursor's state record through
 * CURSOR_STATE_OFF exactly as objCurDraw right after it does.
 */
void objCur(CursorObject *cursor);
void objCurDraw(CursorObject *cursor);

/*
 * Further evidenced words on the cursor's state record (CURSOR_STATE_OFF,
 * the object's own work pointer):
 *   state + 0x04 and state + 0x78 are cleared when the cursor is created;
 *   state + 0x80 is the target address curSel below compares and updates;
 *   state + 0x90 starts the three coordinate floats the caller supplies.
 * The 0x40 bit objEnCurCreate sets in the flags word at STATE_FLAGS_OFF is
 * the enemy-side bit objCurDraw reads back to pick objDispEnCur.
 */
#define STATE_UNK04_OFF 0x04
#define STATE_UNK78_OFF 0x78
#define STATE_TARGET_OFF 0x80
#define STATE_POS_OFF 0x90

ObjectTask *objEnCurCreate(float x, float y, float z)
{
    ObjectTask *task;
    CursorCommandState *state;
    float *pos;

    task = objEntry2(objCur, (void (*)(ObjectTask *))objCurDraw);
    state = task->work;
    objStdInit(task);
    pos = (float *)((char *)state + STATE_POS_OFF);
    pos[0] = x;
    pos[1] = y;
    pos[2] = z;
    *(unsigned int *)((char *)state + STATE_FLAGS_OFF) |= 0x40;
    *(int *)((char *)state + STATE_UNK04_OFF) = 0;
    *(void **)((char *)state + STATE_TARGET_OFF) = 0;
    *(int *)((char *)state + STATE_UNK78_OFF) = 0;
    return task;
}

/*
 * objRemove is defined by ov01/tu001 obj.c; menu.c already redeclares it
 * the same way to call it from a different TU (src/ov01/obj.h is that
 * TU-local header and is not includable here).
 */
extern void objRemove(ObjectTask *task);
extern ObjectTask *pTaskEnCur;

void objCurRemove(void)
{
    if (pTaskEnCur != 0) {
        objRemove(pTaskEnCur);
        pTaskEnCur = 0;
    }
}

typedef signed int s32;

void objCur(CursorObject *cursor)
{
  s32 *temp_17;
  int new_var2;
  u32 temp_3;
  void *temp_2;
  char *pos;
  int new_var;
  temp_17 = *((int *) (((char *) cursor) + 0x10));
  temp_2 = objCmdPtrGet(cursor);
  temp_3 = *((int *) (((char *) temp_2) + 0x0));
  pos = ((char *) temp_17) + 0x90;
  if (temp_3 == 1)
  {
    goto case1;
  }
  if (new_var2 = temp_3 == 0)
  {
    return;
  }
  if (temp_3 < 4)
  {
    new_var = *((int *) (((char *) temp_2) + 0x4));
    if (!((*temp_17) & 4))
    {
      sefGetNullPosition(pos, *((int *) (((char *) new_var) + 0x14)), 0U, 0);
    }
    *temp_17 &= ~2;
  }
  return;
  case1:
  objHatoVec(cursor, 2);

  if (objStdMove(cursor) != 0)
  {
    *temp_17 = (*temp_17) & (~2);
    *((int *) (((char *) temp_2) + 0x0)) = 0U;
  }
}

extern void objDispPlCur(void);
extern void objDispEnCur(CursorObject *cursor);

/*
 * cursor + 0x10 (CURSOR_STATE_OFF) reaches the same state record curCmdSet
 * writes; this function reads back state + 0x00 (STATE_FLAGS_OFF) and
 * returns without drawing when bit 0x4 is set, otherwise dispatches to the
 * enemy-side draw callee (bit 0x40 set) or the player-side one (objDispPlCur,
 * which takes no argument).
 */
void objCurDraw(CursorObject *cursor)
{
    CursorCommandState *state;
    unsigned int flags;

    state = *(CursorCommandState **)((char *)cursor + CURSOR_STATE_OFF);
    flags = *(unsigned int *)((char *)state + STATE_FLAGS_OFF);
    if (flags & 4) {
        return;
    }
    if (!(flags & 0x40)) {
        objDispPlCur();
        return;
    }
    objDispEnCur(cursor);
}

/*
 * Evidenced word offsets on the opaque state/cursor handles. Each offset is
 * observed directly in the original body; the labels stay neutral because
 * the historical member names are unresolved:
 *   cursor + 0x10 : state pointer (lw s1,16(a0) in the callee delay slot);
 *   state  + 0x00 : flags word (read, OR mask 0x2, written back);
 *   state  + 0x70 : word zeroed on entry (sw zero,112(s1));
 *   state  + 0x74 : word zeroed on entry (sw zero,116(s1)).
 * Accesses are written inline (no helper calls) so the candidate emits only
 * the single curCmdSet extent; the macros keep the offsets reviewable.
 */

void curCmdSet(CursorObject *cursor, const CursorCommand *command)
{
    CursorCommandState *state;
    CursorCommand *tail;
    unsigned int flags;

    state = *(CursorCommandState **)((char *)cursor + CURSOR_STATE_OFF);
    tail = objCmdTailGet(cursor);
    *(unsigned int *)((char *)state + STATE_UNK70_OFF) = 0;
    *(unsigned int *)((char *)state + STATE_UNK74_OFF) = 0;
    *tail = *command;
    if (tail->selector != 0) {
        flags = *(unsigned int *)((char *)state + STATE_FLAGS_OFF);
        *(unsigned int *)((char *)state + STATE_FLAGS_OFF) = flags | 2;
    }
}

/*
 * Battle cursor selection list. entries[] holds the target address of each
 * selectable item, indexed by curIdx (word width, stride 4); count bounds
 * curIdx on both ends; prevIdx is a snapshot of curIdx taken on entry and
 * compared again after the target update to decide whether to play the
 * selection sound. Earlier bytes are unmodeled; entries[] is sized to the
 * gap its own offset leaves before count (0x24..0x47, stride 4 = 9 words).
 */
typedef struct CursorSelectList {
    unsigned char unmodeled_00[0x24];
    void *entries[9]; /* +0x24..+0x47 */
    int count;         /* +0x48 */
    int curIdx;         /* +0x4C */
    int prevIdx;         /* +0x50 */
} CursorSelectList;

/*
 * dataPadRead is defined by ov01/tu005 data_unit_org_get.c (still asm);
 * declared here to call it from this TU.
 */
extern int dataPadRead(int padNo);

/*
 * curMove2Pos is defined later in this TU (still asm); forward-declared to
 * call it here.
 */
extern void curMove2Pos(CursorObject *cursor, CursorSelectList *list);

/*
 * sndSysSePlay is defined by ov01/tu013 snd.c (still asm); other TUs already
 * redeclare it the same way to call it from a different TU.
 */
extern int sndSysSePlay(int soundId);

/*
 * +0x80 on the cursor's state record (CURSOR_STATE_OFF above): the target
 * address curMove2Pos last moved the cursor toward.
 */
#define STATE_TARGET_OFF 0x80

int curSel(CursorObject *cursor, CursorSelectList *list, int nextMask, int prevMask)
{
    CursorCommandState *state;
    void *target;

    list->prevIdx = list->curIdx;
    if (dataPadRead(1) & nextMask) {
        list->curIdx++;
        if (list->curIdx >= list->count) {
            list->curIdx = 0;
        }
    } else if (dataPadRead(1) & prevMask) {
        list->curIdx--;
        if (list->curIdx < 0) {
            list->curIdx = list->count - 1;
        }
    }

    state = *(CursorCommandState **)((char *)cursor + CURSOR_STATE_OFF);
    target = list->entries[list->curIdx];
    if (*(void **)((char *)state + STATE_TARGET_OFF) != target) {
        *(void **)((char *)state + STATE_TARGET_OFF) = target;
        curMove2Pos(cursor, list);
        if (list->curIdx != list->prevIdx) {
            sndSysSePlay(0xA);
        }
    }
    return list->curIdx;
}

int curGrpSel(void *cursor, int *grp, int maskA, int maskB) {
    if ((dataPadRead(1) & maskA) || (dataPadRead(1) & maskB)) {
        if (grp[0x58 / 4] == 0) {
            grp[0x58 / 4] = 1;
        } else {
            grp[0x58 / 4] = 0;
        }
        sndSysSePlay(0xA);
        return 1;
    }
    return 0;
}

typedef struct CurUnitList { ObjectTask *units[9]; } CurUnitList;
extern void sefGetNullPosition(void *out, int id, unsigned int a, int b);
extern float *dataPosTblGet(int index);

void curMove2Pos(CursorObject *cursor, CursorSelectList *list)
{
    unsigned int *obj;
    float *pos;
    float *v;
    int index;
    signed char lvl;
    float f;
    CursorCommand cmd;

    obj = *(unsigned int **)((char *)cursor + 0x10);
    pos = (float *)(obj + 0x24);
    if (*(int *)((char *)list + 0x54) == 0) {
        sefGetNullPosition(obj + 0x3C, *(int *)((char *)((CurUnitList *)list)->units[list->curIdx] + 0x14), 0, 0);
        lvl = ((signed char *)calcUPGet(((CurUnitList *)list)->units[list->curIdx]))[0x15];
        if (lvl < 3) {
            pos[5] = 1.0f;
        } else if (lvl < 4) {
            pos[5] = 0.0f;
        } else {
            pos[5] = 2.5f;
        }
    } else {
        index = (int)list->entries[list->curIdx];
        v = dataPosTblGet(index);
        f = v[0];
        pos[0x18] = f;
        pos[0x19] = v[1];
        pos[0x1A] = v[2];
        if (index < 0) {
            pos[0x18] = -f;
        }
        pos[5] = 0.0f;
    }
    if (obj[0] & 4) {
        pos[0] = pos[0x18];
        pos[1] = pos[0x19];
        pos[2] = pos[0x1A];
    }
    cmd.selector = 1;
    curCmdSet(cursor, &cmd);
}

typedef struct CurEnList {
    ObjectTask *units[9];
    int pos[9];
    int count;
    int curIdx;
} CurEnList;
extern struct CurSelGrp curMove;
extern char D_00A442C8[];

int curEnProcInit(ObjectTask **target, int side)
{
    ObjectTask *task = pTaskEnCur;
    CurEnList *list = (CurEnList *)&curMove;
    ObjectTask **table[1];   /* not `&table`: gcc 2.96 leaves the ADDRESSOF mem's alias set uninitialized */
    int count;
    int i = 0;
    int ret;

    list->curIdx = 0;
    list->count = 0;
    count = unitTblGet(side, (BattleUnit ***)table);
    for (; i < count; i++) {
        ObjectTask *u = table[0][i];
        if (u == 0) continue;
        if (*(int *)*(int *)((char *)u + 0x10) & 0x80) continue;
        if (*target == u) list->curIdx = list->count;
        list->units[list->count] = table[0][i];
        list->pos[list->count] = *(short *)((char *)calcUPGet(table[0][i]) + 0x44);
        list->count++;
    }
    if (list->count == 0) {
        printf(D_00A442C8);
        ret = 0;
    } else {
        int idx = list->curIdx;
        ret = 1;
        *(int *)((char *)task->work + 0x78) = 1;
        *target = list->units[idx];
    }
    return ret;
}

int curEnProc(int *arg0)
{
    ObjectTask *cursor = pTaskEnCur;
    CursorSelectList *list = (CursorSelectList *)&curMove;
    int flags;
    int target;
    int curIdx;
    int changed;
    CursorCommand cmd;
    int mask = 0x8000;
    int *work;
    int *elem;

    *(void **)((char *)&curMove + 0x54) = 0;
    *(unsigned int *)cursor->work |= 8;
    curSel((CursorObject *)cursor, list, 0x2000, mask);
    curIdx = list->curIdx;
    changed = (list->prevIdx != curIdx);
    work = (int *)cursor->work;
    flags = *work;
    elem = (int *)&curMove + curIdx;
    target = *elem;
    *(void **)arg0 = (void *)target;
    if ((flags & 2) == 0) {
        cmd.selector = 2;
        cmd.argument = (int)target;
        curCmdSet((CursorObject *)cursor, &cmd);
    }
    return changed;
}

typedef struct CurPosList {
    unsigned char unmodeled_00[0x24];
    int entries[9];
    int count;
    int curIdx;
    int prevIdx;
    int unk54;
} CurPosList;

extern char chaTbl_00A3D7E0[];
extern char agwsTbl_00A3D800[];
extern int calcMoveChk(ObjectTask *unit, int move);
extern char D_00A442E0[];

int curPosProcInit(ObjectTask *arg0, int arg1)
{
    ObjectTask *task = pTaskEnCur;
    CurPosList *list = (CurPosList *)&curMove;
    BattleUnit **table;
    int *var;
    int j;
    int *cnt;
    int count;
    int i;
    int ret;

    if (arg1 == 5) {
        var = (calcUPGet(arg0)->flags & 0x40) ? (int *)chaTbl_00A3D7E0 : (int *)agwsTbl_00A3D800;
    } else {
        var = (calcUPGet(arg0)->flags & 0x40) ? (int *)agwsTbl_00A3D800 : (int *)chaTbl_00A3D7E0;
    }
    list->curIdx = 0;
    list->count = 0;
    count = unitTblGet(2, &table);
    if (var[0] != 0) {
        j = 0;
        cnt = &list->count;
        do {
            if (arg1 != 5 && !calcMoveChk(arg0, var[j])) {
                goto next;
            }
            for (i = 0; i < count && (table[i] == 0 || var[j] != *(short *)((char *)calcUPGet(table[i]) + 0x44)); i++) {
            }
            if (i == count) {
                int idx;
                if (var[j] == *(int *)((char *)task->work + 0x80)) {
                    list->curIdx = list->count;
                }
                idx = *cnt;
                list->entries[idx] = var[j];
                *cnt = idx + 1;
            }
next:
            j++;
        } while (var[j] != 0);
    }
    if (list->count == 0) {
        printf(D_00A442E0);
        ret = 0;
    } else {
        ret = 1;
    }
    *(int *)((char *)task->work + 0x78) = 0;
    return ret;
}

extern struct CurSelGrp curMove;
int curPosProc(CursorObject *unused, int *pos)
{
    ObjectTask *task = pTaskEnCur;
    CurPosList *list = (CurPosList *)&curMove;
    CursorCommand cmd;
    int changed;

    list->unk54 = 1;
    *(unsigned int *)*(int *)((char *)task + 0x10) &= ~8;
    curSel((CursorObject *)task, (CursorSelectList *)list, 0x2000, 0x8000);
    changed = list->prevIdx != list->curIdx;
    if (!(*(unsigned int *)task->work & 2)) {
        cmd.selector = 1;
        cmd.argument = list->entries[list->curIdx];
        curCmdSet((CursorObject *)task, &cmd);
    }
    *pos = list->entries[list->curIdx];
    return changed;
}

INCLUDE_ASM("asm/nonmatchings/ov01/battle_init", curTgtProcInit);

typedef struct CurTgtList {
    void *units[9];
    unsigned char unmodeled_24[0x24];
    int count;
    int curIdx;
    int prevIdx;
    int unk54;
} CurTgtList;
typedef union {
    unsigned int u;
    void *p;
} CurTgtFlags;

int curTgtProc(int *pos, int flags)
{
  ObjectTask *task = pTaskEnCur;
  void *new_var;
  void *work = task->work;
  float *f = (float *) (((char *) work) + 0x90);
  CurTgtList *list = (CurTgtList *) (&curMove);
  CursorCommand cmd;
  int i;
  int ret = 0;
  int fl;
  void *temp;
  signed char lvl;
  if (!(flags & 0x10))
  {
    ((CurTgtFlags *) (&list->unk54))->u = 0;
    ((CurTgtFlags *) task->work)->u &= ~8;
    curSel((CursorObject *) task, (CursorSelectList *) list, 0x2000, 0x8000);
    ret = list->prevIdx != list->curIdx;
    fl = ((CurTgtFlags *) task->work)->u;
    temp = list->units[list->curIdx];
    *pos = (int) temp;
    if (!(fl & 2))
    {
      cmd.selector = 2;
      cmd.argument = (int) temp;
      curCmdSet((CursorObject *) task, &cmd);
    }
  }
  else
  {
    ((CurTgtFlags *) work)->u &= ~8;
    ret = 0;
    if (!(((CurTgtFlags *) task->work)->u & 2))
    {
      i = 0;
      while (i < list->count)
      {
        if (((int) list->units[i]) == (*((int *) (((char *) task->work) + 0x7C))))
        {
          if (!(frameCntGet() & 3))
          {
            i++;
          }
          break;
        }
        i++;
      }

      if (i >= list->count)
      {
        i = 0;
      }
      cmd.selector = 3;
      cmd.argument = (int) list->units[i];
      curCmdSet((CursorObject *) task, &cmd);
      *((void **) (((char *) task->work) + 0x7C)) = (new_var = list->units[i]);
      lvl = ((signed char *) calcUPGet((ObjectTask *) new_var))[0x15];
      if (lvl < 3)
      {
        f[5] = 1.0f;
      }
      else
        if (lvl >= 4)
      {
        f[5] = 2.5f;
      }
      else
      {
        f[5] = 0.0f;
      }
    }
    ret = 0;
  }
  return ret;
}

/*
 * The battle cursor's selection-group record (data still asm-owned, splat
 * name kept); curSelGrpSet is the only known writer of the evidenced word.
 */
typedef struct CurSelGrp {
    unsigned char unmodeled_00[0x58];
    u32 group; /* +0x58: written 0 or 1 by curSelGrpSet */
} CurSelGrp;
extern CurSelGrp curMove;

/*
 * Written -1 whenever curSelGrpSet derives the group from flags instead of
 * receiving it directly (data still asm-owned; original ELF symbol name); no other
 * reference is evidenced in this TU.
 */
extern int oldCurGrp;

void curSelGrpSet(u32 group, int flags)
{
    CurSelGrp *state = &curMove;

    if (group < 2U) {
        state->group = group;
        return;
    }
    if (flags & 8) {
        state->group = 0;
    } else if (flags & 0x40) {
        state->group = 0;
    } else {
        state->group = 1;
    }
    oldCurGrp = -1;
}

/*
 * The current selection group (data still asm-owned, splat name kept); a
 * different object from CurSelGrp above, whose writer is not in this TU.
 */
extern int D_00A57C48;

int curSelGrpGet(void)
{
    return D_00A57C48;
}

void objDispPlCur(void)
{
}

extern void curTexTrans();
extern void objDispCur(ObjectTask *task, int isPl);
extern void menuStatTexTrans(void);
extern BattleUnit *actUnitGet(void);
extern unsigned int frameCntGet(void);
extern void grGpInit(void *packet);
extern void grPacketSend(void *packet);
extern void menuActCurPut(void *packet, CursorObject *cursor, BattleUnit *unit, int frame);
extern void monsNameDisp(void *packet, BattleUnit *unit, int x, int y, int isEn);

void objDispEnCur(CursorObject *cursor)
{
    int packet[4];
    int mode;
    int isEn;
    int flags;

    curTexTrans(0);
    objDispCur((ObjectTask *)cursor, 0);
    menuStatTexTrans();
    if (tgtUnitGet() != 0 && (*(int *)tgtUnitGet()->work & 0x40)) {
        grGpInit(packet);
        menuActCurPut(packet, cursor, actUnitGet(), (frameCntGet() >> 1) & 7);
    }
    if (tgtUnitGet() == 0) {
        return;
    }
    mode = *(int *)((char *)((ObjectTask *)cursor)->work + 0x78);
    if (mode != 1 || battleCtrlHelpGet() == 2) {
        return;
    }
    flags = *(int *)tgtUnitGet()->work & 0x40;
    isEn = flags != 0;
    if (isEn != mode || (tgtUnitGet() != 0 && !(*(int *)tgtUnitGet()->work & 0x80))) {
        grGpInit(packet);
        monsNameDisp(packet, tgtUnitGet(), 8, 0x5c, isEn);
        grPacketSend(packet);
    }
}

extern void curEnvMake(void *packet, int mode);
extern void objDispCurSubEn(void *packet, ObjectTask *task, int line);
extern void curPutSub(void *packet, ObjectTask *task);
void objDispCurSubPl(void *packet, ObjectTask *task);
extern void grGpInit(void *packet);
extern void grPacketSend(void *packet);

void objDispCur(ObjectTask *task, int isPl)
{
    int packet[2];
    int mode;
    int flags;

    grGpInit(packet);
    if (isPl == 0) {
        objCmdPtrGet(task);
        if ((*(int *)((BattleUnit *)task)->work & 8) && tgtUnitGet() != 0
            && (isPl = 1, calcLineChk((ObjectTask *)tgtUnitGet()) == 0)) {
            mode = 0;
        } else {
            isPl = 0;
            flags = *(int *)tgtUnitGet()->work & 0x40;
            mode = flags == 0;
        }
        curEnvMake(packet, mode);
        objDispCurSubEn(packet, task, isPl);
        curPutSub(packet, task);
    } else {
        objDispCurSubPl(packet, task);
    }
    grPacketSend(packet);
}

void objDispCurSubPl(void *packet, ObjectTask *task)
{
}

typedef struct CurSubEnRgba {
    unsigned char c[4];
} CurSubEnRgba;
typedef struct CurSubEnSpr {
    float x0, y0, z0, pad0;
    float x1, y1, z1, pad1;
    CurSubEnRgba col;
    short u0, u1, v0, v1;
} CurSubEnSpr;
typedef struct CurSubEnCol {
    CurSubEnRgba c[3];
} CurSubEnCol;
extern CurSubEnCol D_00A442F8;
extern int grBBCalc(ObjectTask *task, int *x, int *y);
extern void grOpenSpr(void *packet);
extern void grPutSpr(void *packet, CurSubEnSpr *spr, int flag);
extern void grCloseSpr(void);

void objDispCurSubEn(void *packet, ObjectTask *task, int line)
{
    CurSubEnSpr spr;
    CurSubEnCol cols;
    int x;
    int y;
    unsigned int frame;
    int w;
    int h;
    int u;
    int v;

    cols = D_00A442F8;
    if (grBBCalc(task, &x, &y) != 0) {
        w = 0x24;
        h = 0x32;
        x -= 0x12;
        y -= 0x32;
        frame = (frameCntGet() >> 2) % 6;
        u = (int)frame % 3 * w;
        v = (int)frame / 3 * h;
        spr.z0 = 16777212.0f;
        spr.x0 = x;
        spr.y0 = y;
        spr.x1 = x + w;
        spr.y1 = y + h;
        spr.z1 = 16777212.0f;
        spr.u0 = u;
        spr.v0 = v;
        spr.u1 = u + w + 1;
        spr.v1 = v + h + 1;
        __builtin_memcpy(&spr.col, (unsigned char *)&cols + line * 4, 4);
        grOpenSpr(packet);
        grPutSpr(packet, &spr, 0);
        grCloseSpr();
    }
}

extern char cur1[];
extern void grCalcMatrix(void *command, void *destination);
extern void grGsRegSet(void *packet, int reg, int value);
extern void grOpenFT4STQ(void *packet);
extern void grCloseFT4STQ(void);
extern void grPutFT4STQ(void *packet, void *prim, void *matrix);
typedef struct { long long q[16]; } CurBlk128;

void curPutSub(void *packet, ObjectTask *task) {
    CurBlk128 save;
    float mat[16];
    char *rec;
    unsigned int angle;
    float f;

    rec = (char *)task->work + 0x90;
    save = *(CurBlk128 *)rec;
    *(float *)(rec + 0x14) = 0.01f;
    *(float *)(rec + 0x30) = 1.0f;
    *(float *)(rec + 0x34) = 1.0f;
    *(float *)(rec + 0x38) = 1.0f;
    *(int *)(rec + 0x4) = 0;
    angle = frameCntGet() % 360U;
    f = 0.017453292f;
    *(float *)(rec + 0x24) = f * (float)angle;
    grCalcMatrix(task, mat);
    grGsRegSet(packet, 0x14, 0x60);
    grGsRegSet(packet, 0x42, 0x44);
    grOpenFT4STQ(packet);
    grPutFT4STQ(packet, cur1, mat);
    grCloseFT4STQ();
    *(CurBlk128 *)rec = save;
}

/*
 * grGpInit's own record (src/ov01/gr_gp_init.c, still asm there; that TU
 * owns the layout and has not published it through a header yet -- shared-
 * header need). curTexTrans only ever takes this local's address and never
 * reads it back, so the storage is left untyped here rather than restating
 * the owner's struct.
 */
extern void grGpInit(void *packet);
extern void grGsRegSet(void *packet, int reg, int value);
extern void grPacketSend(void *packet);

/*
 * dataXtxFileGet's own table entry (src/ov01/data_unit_org_get.c, still
 * asm); only the word at +0x4 is evidenced here, forwarded unchanged to
 * xglPacketTextureTrans.
 */
typedef struct XtxFile {
    unsigned char unmodeled_00[4];
    int textureId; /* +0x4 */
} XtxFile;

extern XtxFile *dataXtxFileGet(int index);
extern void xglPacketTextureTrans(int textureId);

#define CURSOR_XTX_SLOT 2
/* GS register 0x3F (TEXFLUSH): any write flushes the texture cache after a
 * texture upload, matching the dataXtxFileGet(2) transfer just above it. */
#define GS_REG_TEXFLUSH 0x3F

void curTexTrans(void)
{
    int packet[2]; /* grGpInit's own record: a data pointer and an element
                     * count (this call site's own evidence for the size). */

    xglPacketTextureTrans(dataXtxFileGet(CURSOR_XTX_SLOT)->textureId);
    grGpInit(packet);
    grGsRegSet(packet, GS_REG_TEXFLUSH, 1);
    grPacketSend(packet);
}

extern void GS_REG_SET(void *packet, long long register_id, long long control);
extern unsigned short dataXtxAdrGet(int index);

void curEnvMake(void *packet, int mode) {
    unsigned short tbp;
    unsigned short cbp;

    if (mode == 0) {
        tbp = dataXtxAdrGet(0xF);
        cbp = dataXtxAdrGet(0x10);
    } else {
        tbp = dataXtxAdrGet(0xF);
        cbp = dataXtxAdrGet(0x12);
    }
    GS_REG_SET(packet, 0xE, 0x1000000000008005LL);
    GS_REG_SET(packet, 6, (long long)tbp | ((long long)cbp << 37) | 0x20000005DD340000LL);
    GS_REG_SET(packet, 0x42, 0);
    GS_REG_SET(packet, 0x47, 0x5000D);
    GS_REG_SET(packet, 8, 0);
    GS_REG_SET(packet, 0x14, 0);
}

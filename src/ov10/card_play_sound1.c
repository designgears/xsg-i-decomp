/*
 * OV10 original TU 1: 0x00a00410..0x00a08670 (15 functions)
 */
#include "common.h"
#include "shared.h"
#include "card_play_sound1.h"

struct CardSoundPath {
    char name[34];
};

extern const struct CardSoundPath D_00A4BBD0;
extern const struct CardSoundPath D_00A4BBF8;
extern const struct CardSoundPath D_00A4BC20;
extern const struct CardSoundPath D_00A4BC48;
extern const struct CardSoundPath D_00A4BC70;
extern const struct CardSoundPath D_00A4BC98;
extern int SsdSpuDmaCompleted(int wait);
extern int xglSoundSendSwd(void *swd, int bank);
extern void xglSoundSendSmd2(void *smd, int bank);
extern void xglSoundSequenceNormal2(int channel, int volume);
extern char BGFileNameList[6][16];
extern char D_00A4BDA0[];
extern char D_00A4BDA8[];
extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);

void CardPlaySound1(void)
{
    struct CardSoundPath swdFile = D_00A4BBD0;
    struct CardSoundPath smdFile = D_00A4BBF8;

    if (xglCdReadFile(swdFile.name, (void *) 0x01900000, 0, 0) > 0) {
        xglSoundSendSwd((void *) 0x01900000, -1);
        while (SsdSpuDmaCompleted(0) != 0) {
        }
    }

    if (xglCdReadFile(smdFile.name, (void *) 0x01900000, 0, 0) > 0)
        xglSoundSendSmd2((void *) 0x01900000, 0);

    xglSoundSequenceNormal2(0, 127);
}

void CardPlaySound3(void)
{
    struct CardSoundPath smdFile = D_00A4BC20;

    if (xglCdReadFile(smdFile.name, (void *) 0x01C60000, 0, 1) > 0)
        xglSoundSendSmd2((void *) 0x01C60000, 0);

    xglSoundSequenceNormal2(0, 127);
}

void CardPlaySound2(void)
{
    struct CardSoundPath swdFile = D_00A4BC48;
    struct CardSoundPath smdFile = D_00A4BC70;

    if (xglCdReadFile(swdFile.name, (void *) 0x01900000, 0, 0) > 0) {
        xglSoundSendSwd((void *) 0x01900000, -1);
        while (SsdSpuDmaCompleted(0) != 0) {
        }
    }

    if (xglCdReadFile(smdFile.name, (void *) 0x01900000, 0, 0) > 0)
        xglSoundSendSmd2((void *) 0x01900000, 0);

    xglSoundSequenceNormal2(0, 127);
}

void CardPlaySound4(void)
{
    struct CardSoundPath smdFile = D_00A4BC98;

    if (xglCdReadFile(smdFile.name, (void *) 0x01C60000, 0, 1) > 0)
        xglSoundSendSmd2((void *) 0x01C60000, 0);

    xglSoundSequenceNormal2(0, 127);
}

extern int CardFread(unsigned int mode, char *filename);
extern char D_00A4BCC0[];
extern char D_00A4BCD0[];
extern char D_00A4BCF0[];
extern unsigned char IDdata[];
extern int printf(const char *, ...);

int CardDataLoad(unsigned short *arg0) {
    char buf[13];
    int i;
    unsigned char *p;
    unsigned char *id;

    if (CardFread(0x01000000, D_00A4BCC0) < 0) {
        printf(D_00A4BCD0);
        return 0;
    }
    *arg0 = *(unsigned short *)0x01000000;
    for (i = 0, p = (unsigned char *)0x01000004; i < 12; i++) {
        buf[i] = *p++;
    }
    buf[i] = 0;
    id = IDdata;
    for (i = 0, p = (unsigned char *)0x01000004; i < 12; i++) {
        if (*p++ != *(unsigned char *)(i + (int)id)) {
            printf(D_00A4BCF0);
            return 0;
        }
    }
    return 0x01000010;
}

typedef signed int s32;

extern s32 CardFread(u32 mode, char *filename);
extern char D_00A4BD00[];

u32 CardGraphicLoad(void) {
    return (u32) ~CardFread(0x0100C000, D_00A4BD00) >> 0x1F;
}

extern void xglMatrixUnit(float (*matrix)[4]);
extern char D_00A4BD10[];
extern char D_00A4BD28[];
extern char D_00A4BD40[];
extern char D_00A4BD58[];
extern char D_00A4BD70[];
extern char D_00A4BD88[];

void CardTitleInit(CardGameWork *work) {
    int i;

    for (i = 15; i >= 0; i--)
        work->models[i].visible = 0;
    work->models[0].visible = 1;
    work->models[15].visible = 1;
    xglMatrixUnit(work->models[4].matrix);
    xglMatrixUnit(work->models[5].matrix);
    xglMatrixUnit(work->models[15].matrix);
    work->rotStageCooldown = 0;
    *(float *)&work->unmodeled_70F4[4] = *(float *)&work->unmodeled_70F4[8] = *(float *)&work->unmodeled_70F4[0] = 0.0f;
    work->models[14].model = (void *)0x01DA0000;
    work->models[14].visible = 1;
    work->models[14].texture = (void *)0x01DB5800;
    work->unmodeled_70F0 = 0;
    work->rotStageId = 0;
    work->unmodeled_70F2[0] = 0;
    xglMatrixUnit(work->models[14].matrix);
    work->models[14].matrix[3][1] = -1.37f;
    work->models[14].matrix[3][2] = 0.1f;
    CardFread(0x01900000, D_00A4BD10);
    CardFread(0x01901000, D_00A4BD28);
    CardFread(0x01902000, D_00A4BD40);
    CardFread(0x01A10050, D_00A4BD58);
    CardFread(0x01A200A0, D_00A4BD70);
    CardFread(0x01A300E0, D_00A4BD88);
}

void CMIBGInit(CardGameWork *work)
{
    char filename[32];

    strcpy(filename, BGFileNameList[work->save->background]);
    strcat(filename, D_00A4BDA0);
    CardFread(0x01E40000, filename);

    strcpy(filename, BGFileNameList[work->save->background]);
    strcat(filename, D_00A4BDA8);
    CardFread(0x01FB0300, filename);
}

extern u8 CardInitialDataPack[];
extern u16 CardCommandPlay[];
extern u8 D_4A6840[];
typedef struct { float x, y, z, w; } __attribute__((aligned(16))) CardMainVec;
typedef int Q16 __attribute__((mode(TI)));
typedef struct { u8 pad[0x5BB0]; u32 a[4]; u32 b[4]; } CardMainSub;
extern float s_asIntensity[4][4];
extern float s_asDirection[4][4];
extern char D_00A4BDB0[];
extern char D_00A4BDD0[];
extern char D_00A4BDF0[];
extern char D_00A4BE08[];
extern char D_00A4BE20[];
extern char D_00A4BE38[];
extern char D_00A4BE50[];
extern char D_00A4BE68[];
extern char D_00A4BE80[];
extern char D_00A4BE98[];
extern char D_00A4BEB0[];
extern char D_00A4BEC8[];
extern char D_00A4BEE0[];
extern char D_00A4BEF8[];
extern char D_00A4BF10[];
extern char D_00A4BF20[];
extern char D_00A4BF38[];
extern char D_00A4BF50[];
extern char D_00A4BF68[];
extern char D_00A4BF80[];
extern char D_00A4BF90[];
extern char D_00A4BFA0[];
extern char D_00A4BFB0[];
extern char D_00A4BFC0[];
extern char D_00A4BFD8[];
extern char D_00A4BFE8[];
extern char D_00A4C000[];
extern char D_00A4C018[];
extern char D_00A4C030[];
extern char D_00A4C048[];
extern char D_00A4C060[];
extern char D_00A4C078[];
extern char D_00A4C090[];
extern char D_00A4C0A8[];
extern char D_00A4C0C0[];
extern char D_00A4C0D8[];
extern char D_00A4C0F0[];
extern char D_00A4C100[];
extern char D_00A4C110[];
extern char D_00A4C120[];
extern char D_00A4C130[];
extern char D_00A4C140[];
extern char D_00A4C150[];
extern char D_00A4C160[];
extern char D_00A4C170[];
extern char D_00A4C180[];
extern char D_00A4C190[];
extern char D_00A4C1A0[];
extern char D_00A4C1B0[];
extern char D_00A4C1C0[];
extern char D_00A4C1D0[];
extern char D_00A4C1E0[];
extern char D_00A4C1F8[];
extern char D_00A4C210[];
extern char D_00A4C218[];
extern char D_00A4C220[];
extern char D_00A4C228[];
extern char D_00A4C230[];
extern char D_00A4C238[];
extern char D_00A4C240[];
extern char D_00A4C248[];
extern char D_00A4C250[];
extern char D_00A4C258[];
extern char D_00A4C260[];
extern char D_00A4C268[];
extern char D_00A4C270[];
extern char D_00A4C278[];
extern char D_00A4C280[];
extern char D_00A4C288[];
extern char D_00A4C290[];
extern char D_00A4C298[];
extern char D_00A4C2A0[];
extern char D_00A4C2A8[];
extern char D_00A4C2B0[];
extern char D_00A4C2C0[];
extern char D_00A4C2D0[];
extern char D_00A4C2E0[];
extern char D_00A4C2F0[];
extern char D_00A4C300[];
extern char D_00A4C310[];
extern char D_00A4C320[];
extern char D_00A4C330[];
extern char D_00A4C340[];
extern char D_00A4C350[];
extern char D_00A4C360[];
extern char D_00A4C370[];
extern char D_00A4C380[];
extern char D_00A4C390[];
extern char D_00A4C3A0[];
extern char D_00A4C3B0[];
extern char D_00A4C3C0[];
extern char D_00A4C3D0[];
extern char D_00A4C3E0[];
extern char D_00A4C3F0[];
extern char D_00A4C400[];
extern char D_00A4C408[];
extern char D_00A4C410[];
extern char D_00A4C418[];
extern char D_00A4C420[];
extern char D_00A4C428[];
extern char D_00A4C430[];
extern char D_00A4C438[];
extern char D_00A4C440[];
extern char D_00A4C448[];
extern char D_00A4C450[];
extern char D_00A4C458[];
extern char D_00A4C460[];
extern char D_00A4C468[];
extern char D_00A4C470[];
extern char D_00A4C478[];
extern char D_00A4C480[];
extern char D_00A4C488[];
extern char D_00A4C490[];
extern char D_00A4C498[];
extern char D_00A4C4A0[];
extern char D_00A4C4B0[];
extern char D_00A4C4C0[];
extern char D_00A4C4D0[];
extern char D_00A4C4E0[];
extern char D_00A4C4F0[];
extern char D_00A4C500[];
extern char D_00A4C510[];
extern char D_00A4C520[];
extern char D_00A4C530[];
extern char D_00A4C540[];
extern char D_00A4C550[];
extern char D_00A4C560[];
extern char D_00A4C570[];
extern char D_00A4C580[];
extern char D_00A4C590[];
extern char D_00A4C5A0[];
extern char D_00A4C5B8[];
extern char D_00A4C5D0[];
extern char D_00A4C5E8[];
extern char D_00A4C600[];
extern char D_00A4C618[];
extern char D_00A4C630[];
extern char D_00A4C648[];
extern char D_00A4C660[];
extern char D_00A4C678[];
extern char D_00A4C690[];
extern char D_00A4C6A0[];
extern char D_00A4C6B8[];
extern char D_00A4C6D0[];
extern char D_00A4C6E8[];
extern char D_00A4C700[];
extern char D_00A4C718[];
extern char D_00A4C730[];
extern char D_00A4C748[];
extern int CardAddList(void *o, int idx);
extern void xglMatrixScale(float (*dst)[4], float (*src)[4], float *scale);
extern void CardHelpFileLoad(void);
extern void InitCpuDeck(void);
extern void xglLightIntensityParallel(void *light, int no, void *color);
extern void xglLightAngle(void *light, int no, void *dir);
static inline void CardMainQCopy(void *s, void *d)
{
  do
  {
    volatile Q16 *sp = (volatile Q16 *) s;
    volatile Q16 *dp = (volatile Q16 *) d;
    *dp = *sp;
  }
  while (0);
}

s32 CardMainInit(CardGameWork *work)
{
  char filename[32];
  float scale[4];
  CardGameWork *new_var;
  StudioLight *light;
  CardMainVec campos __attribute__((aligned(16)));
  CardSaveData *save;
  float (*mat)[4];
  int i;
  int j;
  int sum;
  u8 b;
  int defs;
  CardModelEntry *entry;
  CardMainSub *psave;
  u8 *bits;
  save = (CardSaveData *) D_4A6840;
  work->save = save;
  if (!((1 & save->flags) & 0xFF))
  {
    save->flags |= 1;
    sum = 0;
    i = 0;
    work->save->flags |= 0x20;
    do
    {
      b = CardInitialDataPack[i];
      *((u8 *) ((((char *) i) + ((int) work->save)) + 0x2C)) = b;
      sum = (sum + b) & 0xFF;
      *((u8 *) ((((char *) i) + ((int) work->save)) + 0x56)) = b;
      *((u8 *) ((((char *) i) + ((int) work->save)) + 0x80)) = b;
      *((u8 *) ((((char *) i) + ((int) work->save)) + 0xAA)) = b;
      *((u8 *) ((((char *) i) + ((int) work->save)) + 0xD4)) = b;
      i++;
      CardAddList(work, b);
    }
    while (i < 0x28);
    sum = (~sum) & 0xFF;
    *((u16 *) (((u8 *) work->save) + 0x2A)) = sum;
    *((u16 *) (((u8 *) work->save) + 0x54)) = sum;
    *((u16 *) (((u8 *) work->save) + 0x7E)) = sum;
    *((u16 *) (((u8 *) work->save) + 0xA8)) = sum;
    *((u16 *) (((u8 *) work->save) + 0xD2)) = sum;
  }
  {
    i = 0;
    bits = &work->save->unmodeled_02[2];
    for (i = 0; i < 150; i++)
    {
      ((u16 *) work->ownedCount)[i] = (bits[i / 4] >> ((i % 4) * 2)) & 3;
    }

  }
  if (work->save->background >= 6)
  {
    work->save->background = 0;
  }
  psave = (CardMainSub *) (&work->save);
  psave = (CardMainSub *) (&work->save);
  for (i = 0; i < 40; i++)
  {
    work->playerDeck[i] = 0xFF;
    work->enemyDeck[i] = 0xFF;
  }

  mat = work->models[0].matrix;
  entry = work->models;
  i = 0xF;
  do
  {
    float (*cur)[4] = mat;
    entry->visible = 0;
    entry->model = 0;
    i--;
    entry->texture = 0;
    entry++;
    mat = (float (*)[4]) (((char *) cur) + 0x50);
    xglMatrixUnit(cur);
  }
  while (i >= 0);
  work->cursor.selectionMode = 0;
  work->cursorZone = 0;
  work->cursorIndex = 0;
  work->highlightX = (work->highlightY = (work->highlightScaleX = (work->highlightScaleY = 1.0f)));
  work->cursorPos[0] = (work->cursorPos[1] = (work->cursorPos[2] = 0.0f));
  work->commandIds = CardCommandPlay;
  *((float *) (&work->unmodeled_5B7C[0])) = 1.0f;
  {
    u32 *zp = psave->b;
    i = 3;
    do
    {
      i--;
      zp[-4] = 0;
      *zp = 0;
      zp++;
    }
    while (i >= 0);
  }
  work->unmodeled_7105[0] = 0;
  work->unmodeled_70F0 = 0;
  work->rotStageCooldown = 0;
  work->unmodeled_70F2[0] = 0;
  work->rotStageId = 0;
  work->fase = 0;
  defs = CardDataLoad((u16 *) &work->cardCount);
  work->definitions = (CardDefinition *) defs;
  if (defs == 0)
  {
    printf(D_00A4BDB0);
    return 0;
  }
  if (((u32 (*)(int, int)) CardGraphicLoad)(defs, *((u16 *) &work->cardCount)) == 0)
  {
    printf(D_00A4BDD0);
    return 0;
  }
  CardFread(0x01D00000, D_00A4BDF0);
  CardFread(0x01D00510, D_00A4BE08);
  CardFread(0x01D00A20, D_00A4BE20);
  CardFread(0x01D00F30, D_00A4BE38);
  CardFread(0x01D01440, D_00A4BE50);
  CardFread(0x01D01950, D_00A4BE68);
  CardFread(0x01D02000, D_00A4BE80);
  CardFread(0x01D02510, D_00A4BE98);
  CardFread(0x01D02A20, D_00A4BEB0);
  CardFread(0x01D02F30, D_00A4BEC8);
  CardFread(0x01D03440, D_00A4BEE0);
  CardFread(0x01D03950, D_00A4BEF8);
  CardFread(0x01D06000, D_00A4BF10);
  CardFread(0x01D08000, D_00A4BF20);
  CardFread(0x01D0C000, D_00A4BF38);
  CardFread(0x01D10000, D_00A4BF50);
  CardFread(0x01D12000, D_00A4BF68);
  CardFread(0x01D14000, D_00A4BF80);
  CardFread(0x01D18000, D_00A4BF90);
  CardFread(0x01D1C000, D_00A4BFA0);
  CardFread(0x01D20000, D_00A4BFB0);
  CardFread(0x01D60000, D_00A4BFC0);
  CardFread(0x01D80000, D_00A4BFD8);
  *((u32 *) (((char *) work) + 0x5BBC)) = 0x01D00000;
  *((u32 *) (((char *) work) + 0x5BC0)) = 0x01D02000;
  *((u32 *) (((char *) work) + 0x5BC4)) = 0x01D08000;
  *((u32 *) (((char *) work) + 0x5BCC)) = 0x01D20000;
  *((u32 *) (((char *) work) + 0x5BD0)) = 0x01D20000;
  *((u32 *) (((char *) work) + 0x5BD4)) = 0x01D60000;
  CardFread(0x01DA0000, D_00A4BFE8);
  CardFread(0x01DA2B00, D_00A4C000);
  CardFread(0x01DA5600, D_00A4C018);
  CardFread(0x01DA8100, D_00A4C030);
  CardFread(0x01DAAC00, D_00A4C048);
  CardFread(0x01DAD700, D_00A4C060);
  CardFread(0x01DB0200, D_00A4C078);
  CardFread(0x01DB2D00, D_00A4C090);
  CardFread(0x01DB5800, D_00A4C0A8);
  CardFread(0x01DDF800, D_00A4C0C0);
  CardFread(0x01DDFC00, D_00A4C0D8);
  CardFread(0x016E0000, D_00A4C0F0);
  CardFread(0x016F8100, D_00A4C100);
  CardFread(0x016F8500, D_00A4C110);
  CardFread(0x016F8900, D_00A4C120);
  CardFread(0x016FD440, D_00A4C130);
  CardFread(0x01701F80, D_00A4C140);
  CardFread(0x01706AC0, D_00A4C150);
  CardFread(0x0170B600, D_00A4C160);
  CardFread(0x0170D600, D_00A4C170);
  CardFread(0x0170F600, D_00A4C180);
  CardFread(0x01713600, D_00A4C190);
  CardFread(0x01714600, D_00A4C1A0);
  CardFread(0x01715600, D_00A4C1B0);
  CardFread(0x01716600, D_00A4C1C0);
  CardFread(0x01717600, D_00A4C1D0);
  CardFread(0x01C73000, D_00A4C1E0);
  CardFread(0x01C74000, D_00A4C1F8);
  CardFread(0x01C80000, D_00A4C210);
  CardFread(0x01C80400, D_00A4C218);
  CardFread(0x01C80800, D_00A4C220);
  CardFread(0x01C80C00, D_00A4C228);
  CardFread(0x01C81000, D_00A4C230);
  CardFread(0x01C81400, D_00A4C238);
  CardFread(0x01C81800, D_00A4C240);
  CardFread(0x01C81C00, D_00A4C248);
  CardFread(0x01C82000, D_00A4C250);
  CardFread(0x01C82400, D_00A4C258);
  CardFread(0x01C82800, D_00A4C260);
  CardFread(0x01C82C00, D_00A4C268);
  CardFread(0x01C83000, D_00A4C270);
  CardFread(0x01C83400, D_00A4C278);
  CardFread(0x01C83800, D_00A4C280);
  CardFread(0x01C83C00, D_00A4C288);
  CardFread(0x01C84000, D_00A4C290);
  CardFread(0x01C84400, D_00A4C298);
  CardFread(0x01C84800, D_00A4C2A0);
  CardFread(0x01C84C00, D_00A4C2A8);
  CardFread(0x01C85000, D_00A4C2B0);
  CardFread(0x01C85400, D_00A4C2C0);
  CardFread(0x01C85800, D_00A4C2D0);
  CardFread(0x01C85C00, D_00A4C2E0);
  CardFread(0x01C86000, D_00A4C2F0);
  CardFread(0x01C86400, D_00A4C300);
  CardFread(0x01C86800, D_00A4C310);
  CardFread(0x01C86C00, D_00A4C320);
  CardFread(0x01C87000, D_00A4C330);
  CardFread(0x01C87400, D_00A4C340);
  CardFread(0x01C87800, D_00A4C350);
  CardFread(0x01C87C00, D_00A4C360);
  CardFread(0x01C88000, D_00A4C370);
  new_var = work;
  CardFread(0x01C88400, D_00A4C380);
  CardFread(0x01C88800, D_00A4C390);
  CardFread(0x01C88C00, D_00A4C3A0);
  CardFread(0x01C89000, D_00A4C3B0);
  CardFread(0x01C89400, D_00A4C3C0);
  CardFread(0x01C89800, D_00A4C3D0);
  CardFread(0x01C89C00, D_00A4C3E0);
  CardFread(0x01C8A000, D_00A4C3F0);
  CardFread(0x01C8A400, D_00A4C400);
  CardFread(0x01C8A800, D_00A4C408);
  CardFread(0x01C8AC00, D_00A4C410);
  CardFread(0x01C8B000, D_00A4C418);
  CardFread(0x01C8B400, D_00A4C420);
  CardFread(0x01C8B800, D_00A4C428);
  CardFread(0x01C8BC00, D_00A4C430);
  CardFread(0x01C8C000, D_00A4C438);
  CardFread(0x01C8C400, D_00A4C440);
  CardFread(0x01C8C800, D_00A4C448);
  CardFread(0x01C8CC00, D_00A4C450);
  CardFread(0x01C8D000, D_00A4C458);
  CardFread(0x01C8D400, D_00A4C460);
  CardFread(0x01C8D800, D_00A4C468);
  CardFread(0x01C8DC00, D_00A4C470);
  CardFread(0x01C8E000, D_00A4C478);
  CardFread(0x01C8E400, D_00A4C480);
  CardFread(0x01C8E800, D_00A4C488);
  CardFread(0x01C8EC00, D_00A4C490);
  CardFread(0x01C8F000, D_00A4C498);
  CardFread(0x01C8F400, D_00A4C4A0);
  CardFread(0x01C8F800, D_00A4C4B0);
  CardFread(0x01C8FD10, D_00A4C4C0);
  CardFread(0x01C90220, D_00A4C4D0);
  CardFread(0x01C90730, D_00A4C4E0);
  CardFread(0x01C91000, D_00A4C4F0);
  CardFread(0x01C94050, D_00A4C500);
  CardFread(0x01C990A0, D_00A4C510);
  CardFread(0x01C9BB70, D_00A4C520);
  CardFread(0x01CA1000, D_00A4C530);
  CardFread(0x01CA1C00, D_00A4C540);
  CardFread(0x01CA2B00, D_00A4C550);
  CardFread(0x01CA4000, D_00A4C560);
  CardFread(0x01CA6000, D_00A4C570);
  CardFread(0x01CB0000, D_00A4C580);
  CardFread(0x01CC0050, D_00A4C590);
  CardFread(0x01930000, D_00A4C5A0);
  CardFread(0x01931800, D_00A4C5B8);
  CardFread(0x01933000, D_00A4C5D0);
  CardFread(0x01934800, D_00A4C5E8);
  CardFread(0x01936000, D_00A4C600);
  CardFread(0x01937800, D_00A4C618);
  CardFread(0x01DE0000, D_00A4C630);
  CardFread(0x01DF0400, D_00A4C648);
  CardFread(0x01DF6000, D_00A4C660);
  CardFread(0x01E00000, D_00A4C678);
  CardFread(0x01E10000, D_00A4C690);
  CardFread(0x01E20000, D_00A4C6A0);
  CardFread(0x01E30000, D_00A4C6B8);
  CardFread(0x01E38000, D_00A4C6D0);
  strcpy(filename, BGFileNameList[new_var->save->background]);
  strcat(filename, D_00A4BDA0);
  CardFread(0x01E40000, filename);
  CardFread(0x01EF0000, D_00A4C6E8);
  CardFread(0x01F30100, D_00A4C700);
  CardFread(0x01996000, D_00A4C718);
  CardFread(0x0199A000, D_00A4C730);
  CardFread(0x01F70200, D_00A4C748);
  strcpy(filename, BGFileNameList[work->save->background]);
  strcat(filename, D_00A4BDA8);
  CardFread(0x01FB0300, filename);
  work->models[0].model = (void *) 0x01DE0000;
  new_var->models[0].texture = (void *) 0x01EF0000;
  new_var->models[1].model = (void *) 0x01DF0400;
  work->models[1].texture = (void *) 0x01F30100;
  work->models[2].model = (void *) 0x01DF6000;
  work->models[2].texture = (void *) 0x01F30100;
  work->models[3].model = (void *) 0x01E00000;
  work->models[3].texture = (void *) 0x01F30100;
  new_var->models[4].model = (void *) 0x01E10000;
  new_var->models[4].texture = (void *) 0x01F70200;
  if (new_var->save->flags & 0x20)
  {
    new_var->models[7].texture = (void *) 0x01F70200;
    work->models[7].model = (void *) 0x01E20000;
    do
    {
    }
    while (0);
    new_var->models[5].model = (void *) 0x01E20000;
    new_var->models[5].texture = (void *) 0x01F70200;
  }
  else
  {
 do { new_var->models[7].model = (void *) 0x01996000; work->models[7].texture = (void *) 0x0199A000; } while (0);
    work->models[5].model = (void *) 0x01996000;
    new_var->models[5].texture = (void *) 0x0199A000;
  }
  new_var->models[6].model = (void *) 0x01E10000;
  if (!work->save)
  {
  }
  work->models[6].texture = (void *) 0x01F70200;
  new_var->models[15].model = (void *) 0x01E40000;
  new_var->models[15].texture = (void *) 0x01FB0300;
  for (i = 0; i < 16; i++)
  {
    xglMatrixUnit(new_var->models[i].matrix);
  }

  work->models[0].matrix[3][1] = 1.4728f;
  scale[0] = 1.13f;
  scale[3] = 1.0f;
  scale[2] = 1.0f;
  scale[1] = 1.0f;
  xglMatrixScale(work->models[1].matrix, new_var->models[1].matrix, scale);
  xglMatrixScale(new_var->models[2].matrix, work->models[2].matrix, scale);
  xglMatrixScale(work->models[3].matrix, work->models[3].matrix, scale);
  CardHelpFileLoad();
  InitCpuDeck();
  CardTitleInit(new_var);
  campos.x = 0.0f;
  campos.y = 0.0f;
  campos.z = 5.8f;
  campos.w = 1.0f;
  save = (CardSaveData *) (&xglStudioSelectGetActiveCamera(0)->position);
  CardMainQCopy(&campos, save);
  j = 0;
  xglStudioGetLight(&light);
  work->fadeOutTimer = 0;
  new_var->modelFadeInTimer = 0;
  new_var->unmodeled_710A[0] = 0x80;
  new_var->fadeOutColor[0] = 0x80;
  new_var->unmodeled_710A[1] = 0x80;
  work->fadeOutColor[1] = 0x80;
  work->unmodeled_710A[2] = 0x80;
  work->fadeOutColor[2] = 0x80;
  work->unmodeled_710A[3] = 0x80;
  new_var->fadeOutColor[3] = 0x80;
  do
  {
    if (j == 0)
    {
      work->modelColor[0][0] = (work->modelColorTarget[0][0] = s_asIntensity[0][0]);
      new_var->modelColor[0][1] = (work->modelColorTarget[0][1] = s_asIntensity[0][1]);
      new_var->modelColor[0][2] = (new_var->modelColorTarget[0][2] = s_asIntensity[0][2]);
      work->modelColor[0][3] = (work->modelColorTarget[0][3] = s_asIntensity[0][3]);
      xglLightIntensityAmbient(light, s_asIntensity);
    }
    else
    {
      xglLightIntensityParallel(light, j - 1, &s_asIntensity[j]);
      xglLightAngle(light, j - 1, s_asDirection[j]);
    }
    j++;
  }
  while (j < 4);
  return 1;
}

void CardMainItemGetSub(int *prizes, int round)
{
    prizes[0] = prizes[1] = prizes[2] = prizes[3] = 0;

    if (round <= 0)
        return;

    {
        int remaining = round;

        do {
            int result = xglSRand();

            if (xglFlagsGet(301, 1) == 0)
                result = 0;
            if (result & 1)
                prizes[1]++;
            else
                prizes[0]++;
            remaining--;
        } while (remaining != 0);
    }
}

extern int CardAddList(void *o, int idx);
extern void dataAccBoxInc(int accessory_id);

void CardMainAddItemSub(CardGameWork *work, int *prizes)
{
    int i;
    u16 *dst;
    u8 *bits;

    i = 0;
    if (prizes[0] > 0) {
        do {
            i++;
            ((int (*)(int))dataEvtBoxInc)(0x36);
        } while (i < prizes[0]);
    }
    i = 0;
    if (prizes[1] > 0) {
        do {
            i++;
            ((int (*)(int))dataEvtBoxInc)(0x37);
        } while (i < prizes[1]);
    }
    if (prizes[2] != 0) {
        if (prizes[2] == 1) {
            CardAddList(work, 1);
        } else {
            CardAddList(work, 8);
        }
    }
    if (prizes[3] != 0) {
        dataAccBoxInc(0x8E);
    }
    dst = (u16 *)work->ownedCount;
    bits = &work->save->unmodeled_02[2];
    for (i = 0; i < 150; i++) {
        int t = bits[i / 4];
        t = (t >> ((i % 4) * 2)) & 3;
        *dst = t;
        dst++;
    }
}

extern char D_00A4C778[];
extern char D_00A4C798[];
extern char D_00A4C7A8[];
extern char D_00A4C7B8[];
extern char D_00A4C7D8[];
extern char D_00A4C7E8[];
extern char D_00A4C7F8[];
extern char D_00A4C808[];
extern char D_00A4C818[];
extern char D_00A4C830[];
extern char D_00A4C848[];
extern char D_00A4C858[];
extern char D_00A4C788[];
extern char D_00A4C870[];
extern float atColor[4][4];
extern float auColor[4][4];
extern char *CardPlayTMessList[];

s32 NewCMPGameModeExit(int arg0, void *arg1)
{
    float matrix[4][4];
    float placement[4][4];
    float position[4];
    float scale[4];
    unsigned short flags;
    s32 ret;
    char *text;
    float zero;

    ret = 0;
    flags = *(unsigned short *)((char *)arg1 + 0x5AD6);
    if (arg0 != 0) {
        flags = *(unsigned short *)((char *)arg1 + 0x5ADE);
    }
    xglFontDebugPrintf(0x18, 0xA, "Build:Nov 24 2002 11:02:50");
    zero = 0.0f;
    position[1] = zero;
    position[0] = zero;
    position[2] = 1.01f;
    position[3] = 1.0f;
    scale[1] = 0.8f;
    scale[0] = 0.8f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
    xglMatrixUnit(matrix);

    if (flags & 0x1000) {
        if (*(short *)((char *)arg1 + 0x5AE6) < 0) {
            if (*(short *)((char *)arg1 + 0x5AE4) > 0) {
                *(short *)((char *)arg1 + 0x5AE4) = *(short *)((char *)arg1 + 0x5AE4) - 1;
                xglSoundEffectNormalID(3, 0);
            }
        } else if (*(short *)((char *)arg1 + 0x5AE6) > 0) {
            *(short *)((char *)arg1 + 0x5AE6) = *(short *)((char *)arg1 + 0x5AE6) - 1;
            xglSoundEffectNormalID(3, 0);
        }
    } else if (flags & 0x4000) {
        if (*(short *)((char *)arg1 + 0x5AE6) < 0) {
            if (*(short *)((char *)arg1 + 0x5AE4) < 3) {
                *(short *)((char *)arg1 + 0x5AE4) = *(short *)((char *)arg1 + 0x5AE4) + 1;
                xglSoundEffectNormalID(3, 0);
            }
        } else if (*(short *)((char *)arg1 + 0x5AE6) < 3) {
            *(short *)((char *)arg1 + 0x5AE6) = *(short *)((char *)arg1 + 0x5AE6) + 1;
            xglSoundEffectNormalID(3, 0);
        }
    }
    if (flags & 0x800) {
        xglSoundEffectNormalID(2, 0);
        ret = -1;
    }
    if (flags & 0x40) {
        if (*(short *)((char *)arg1 + 0x5AE6) < 0) {
            *(short *)((char *)arg1 + 0x5AE4) = 0;
            ret = -1;
            xglSoundEffectNormalID(2, 0);
        } else {
            *(short *)((char *)arg1 + 0x5AE6) = -1;
            xglSoundEffectNormalID(2, 0);
        }
    } else if (flags & 0x20) {
        if (*(short *)((char *)arg1 + 0x5AE6) < 0) {
            switch (*(short *)((char *)arg1 + 0x5AE4)) {
            case 0:
                *((unsigned char *)arg1 + 0x7109) = 0x80;
                xglSoundEffectNormalID(1, 0);
                *((unsigned char *)arg1 + 0x59BC) = 0x30;
                *((char *)arg1 + 0x59BD) = arg0;
                break;
            case 2:
                ret = 1;
                *((unsigned char *)arg1 + 0x7109) = 0x80;
                break;
            case 1:
                xglSoundEffectNormalID(1, 0);
                *(short *)((char *)arg1 + 0x5AE6) = 0;
                break;
            case 3:
                *((unsigned char *)arg1 + 0x7109) = 0x80;
                xglSoundEffectNormalID(1, 0);
                *((unsigned char *)arg1 + 0x7104) = 0x10;
                *(unsigned short *)((char *)arg1 + 0x7102) = 10;
                *(unsigned short *)((char *)arg1 + 0x59B4) = 0;
                break;
            }
        } else {
            switch (*(short *)((char *)arg1 + 0x5AE6)) {
            case 0:
                {
                    unsigned char *q = *(unsigned char **)((char *)arg1 + 0xC);
                    unsigned char v = *q;
                    *q = (v & 0xF7) | ((v + 8) & 8);
                }
                xglSoundEffectNormalID(1, 0);
                break;
            case 1:
                {
                    unsigned char *q = *(unsigned char **)((char *)arg1 + 0xC);
                    unsigned char v = *q;
                    *q = (v & 0xFB) | ((v + 4) & 4);
                }
                xglSoundEffectNormalID(1, 0);
                break;
            case 2:
                {
                    unsigned char *q = *(unsigned char **)((char *)arg1 + 0xC);
                    unsigned char v = *q;
                    *q = (v & 0xFD) | ((v + 2) & 2);
                }
                xglSoundEffectNormalID(1, 0);
                break;
            case 3:
                {
                    unsigned char *q = *(unsigned char **)((char *)arg1 + 0xC);
                    unsigned char v = *q;
                    *q = (v & 0xEF) | ((v + 0x10) & 0x10);
                }
                xglSoundEffectNormalID(1, 0);
                break;
            }
        }
    }

    xglFontPrint(0, 0, 0xFFF0, D_00A4C778);
    if (*(short *)((char *)arg1 + 0x5AE4) == 0) {
        nmlModelSetLight(atColor, asDir);
        xglFontPrint(0, 0, 0xFFF0, D_00A4C780.text);
    } else {
        nmlModelSetLight(auColor, asDir);
    }
    xglFontPrint(0, 0, 0, D_00A4C788);
    xglFontPrint(0xE2, 0x8A, 0xFFF0, D_00A4C790);
    position[1] = 0.58869994f;
    xglMatrixTrans(placement, matrix, position);
    xglMatrixScale(placement, placement, scale);
    nmlModelSetPlace(placement);
    nmlModelSetTexture((void *) 0x0170F600);
    nmlModelEntryCard((void *) 0x0170B600);

    xglFontPrint(0, 0, 0xFFF0, D_00A4C778);
    if (*(short *)((char *)arg1 + 0x5AE4) == 2) {
        xglFontPrint(0, 0, 0xFFF0, D_00A4C780.text);
        nmlModelSetLight(atColor, asDir);
    } else {
        nmlModelSetLight(auColor, asDir);
    }
    xglFontPrint(0, 0, 0, D_00A4C788);
    xglFontPrint(0xD8, 0xAF, 0xFFF0, D_00A4C798);
    position[1] = 0.0098f;
    xglMatrixTrans(placement, matrix, position);
    xglMatrixScale(placement, placement, scale);
    nmlModelSetPlace(placement);
    nmlModelSetTexture((void *) 0x0170F600);
    nmlModelEntryCard((void *) 0x0170B600);

    xglFontPrint(0, 0, 0xFFF0, D_00A4C778);
    if (*(short *)((char *)arg1 + 0x5AE4) == 1) {
        nmlModelSetLight(atColor, asDir);
        xglFontPrint(0, 0, 0xFFF0, D_00A4C780.text);
    } else {
        nmlModelSetLight(auColor, asDir);
    }
    xglFontPrint(0, 0, 0, D_00A4C788);
    xglFontPrint(0xC4, 0xD5, 0xFFF0, D_00A4C7A8);
    position[1] = 0.3f;
    xglMatrixTrans(placement, matrix, position);
    xglMatrixScale(placement, placement, scale);
    nmlModelSetPlace(placement);
    nmlModelSetTexture((void *) 0x0170F600);
    nmlModelEntryCard((void *) 0x0170B600);

    xglFontPrint(0, 0, 0xFFF0, D_00A4C778);
    if (*(short *)((char *)arg1 + 0x5AE4) == 3) {
        xglFontPrint(0, 0, 0xFFF0, D_00A4C780.text);
        nmlModelSetLight(atColor, asDir);
    } else {
        nmlModelSetLight(auColor, asDir);
    }
    xglFontPrint(0, 0, 0, D_00A4C788);
    xglFontPrint(0xCE, 0xFB, 0xFFF0, D_00A4C7B8);
    position[1] = -0.2792f;
    xglMatrixTrans(placement, matrix, position);
    xglMatrixScale(placement, placement, scale);
    nmlModelSetPlace(placement);
    nmlModelSetTexture((void *) 0x0170F600);
    nmlModelEntryCard((void *) 0x0170B600);

    if (*(short *)((char *)arg1 + 0x5AE6) >= 0) {
        position[0] = 1.4374f;
        xglFontPrint(0, 0, 0xFFF0, D_00A4C7C8.text);
        if (*(short *)((char *)arg1 + 0x5AE6) == 0) {
            xglFontPrint(0, 0, 0xFFF0, D_00A4C7D0.text);
            nmlModelSetLight(asColor, asDir);
        }
        xglFontPrint(0, 0, 0, D_00A4C788);
        if (*(unsigned char *)(*(unsigned char **)((char *)arg1 + 0xC)) & 8) {
            xglFontPrint(0x168, 0x9B, 0xFFF0, D_00A4C7D8);
        } else {
            xglFontPrint(0x168, 0x9B, 0xFFF0, D_00A4C7E8);
        }
        position[1] = 0.4415f;
        xglMatrixTrans(placement, matrix, position);
        xglMatrixScale(placement, placement, scale);
        nmlModelSetPlace(placement);
        nmlModelSetTexture((void *) 0x0170F600);
        nmlModelEntryCard((void *) 0x0170D600);

        xglFontPrint(0, 0, 0xFFF0, D_00A4C7C8.text);
        if (*(short *)((char *)arg1 + 0x5AE6) == 1) {
            xglFontPrint(0, 0, 0xFFF0, D_00A4C7D0.text);
            nmlModelSetLight(asColor, asDir);
        }
        xglFontPrint(0, 0, 0, D_00A4C788);
        if (*(unsigned char *)(*(unsigned char **)((char *)arg1 + 0xC)) & 4) {
            xglFontPrint(0x168, 0xC1, 0xFFF0, D_00A4C7F8);
        } else {
            xglFontPrint(0x168, 0xC1, 0xFFF0, D_00A4C808);
        }
        position[1] = 0.1509f;
        xglMatrixTrans(placement, matrix, position);
        xglMatrixScale(placement, placement, scale);
        nmlModelSetPlace(placement);
        nmlModelSetTexture((void *) 0x0170F600);
        nmlModelEntryCard((void *) 0x0170D600);

        xglFontPrint(0, 0, 0xFFF0, D_00A4C7C8.text);
        if (*(short *)((char *)arg1 + 0x5AE6) == 2) {
            xglFontPrint(0, 0, 0xFFF0, D_00A4C7D0.text);
            nmlModelSetLight(asColor, asDir);
        }
        xglFontPrint(0, 0, 0, D_00A4C788);
        if (!(*(unsigned char *)(*(unsigned char **)((char *)arg1 + 0xC)) & 2)) {
            xglFontPrint(0x155, 0xE6, 0xFFF0, D_00A4C818);
        } else {
            xglFontPrint(0x155, 0xE6, 0xFFF0, D_00A4C830);
        }
        position[1] = -0.1376f;
        xglMatrixTrans(placement, matrix, position);
        xglMatrixScale(placement, placement, scale);
        nmlModelSetPlace(placement);
        nmlModelSetTexture((void *) 0x0170F600);
        nmlModelEntryCard((void *) 0x0170D600);

        xglFontPrint(0, 0, 0xFFF0, D_00A4C7C8.text);
        if (*(short *)((char *)arg1 + 0x5AE6) == 3) {
            xglFontPrint(0, 0, 0xFFF0, D_00A4C7D0.text);
            nmlModelSetLight(asColor, asDir);
        }
        xglFontPrint(0, 0, 0, D_00A4C788);
        if (!(*(unsigned char *)(*(unsigned char **)((char *)arg1 + 0xC)) & 0x10)) {
            xglFontPrint(0x168, 0x109, 0xFFF0, D_00A4C848);
        } else {
            xglFontPrint(0x168, 0x109, 0xFFF0, D_00A4C858);
        }
        position[1] = -0.40780002f;
        xglMatrixTrans(placement, matrix, position);
        xglMatrixScale(placement, placement, scale);
        nmlModelSetPlace(placement);
        nmlModelSetTexture((void *) 0x0170F600);
        nmlModelEntryCard((void *) 0x0170D600);
    }

    xglMatrixUnit(matrix);
    position[1] = -1.2f;
    position[0] = zero;
    position[2] = 1.01f;
    scale[1] = 0.85f;
    scale[0] = 0.85f;
    xglMatrixUnit(matrix);
    xglMatrixTrans(matrix, matrix, position);
    xglMatrixScale(matrix, matrix, scale);
    nmlModelSetPlace(matrix);
    nmlModelSetTexture((void *) 0x01DB5800);
    nmlModelEntryCard((void *) 0x01DA0000);
    xglFontPrint(0, 0, 0xFFF0, D_00A4C868);
    if (*(short *)((char *)arg1 + 0x5AE6) < 0) {
        text = CardPlayTMessList[0x46 + *(short *)((char *)arg1 + 0x5AE4)];
    } else {
        text = CardPlayTMessList[0x4B + *(short *)((char *)arg1 + 0x5AE6)];
    }
    CardDispTxtSub(0x18, 0x142, text, 0x17);
    xglFontPrint(0, 0, 0xFFF0, D_00A4C870);
    {
        float zero0 = 0.0f;
        float one0 = 1.0f;

        position[1] = zero0;
        position[0] = zero0;
        position[3] = one0;
        position[2] = one0;
    }
    xglMatrixUnit(matrix);
    xglMatrixTrans(matrix, matrix, position);
    nmlModelSetPlace(matrix);
    nmlModelEntryCard((void *) 0x01D06000);
    return ret;
}

extern u8 D_00A56F10;
extern void nmlModelSetToumei(int enabled);
extern void nmlModelSetAlpha(unsigned int alpha);
extern void nmlModelEntry(int entry);
extern int CardPlayYamaCnt(void *side);
extern void CGPRefreshFieldHP(CardGameWork *work);
static inline void CaedMtxScaleI(float matrix[4][4], float *scale, float x, float y, float z, float w)
{
    xglMatrixUnit(matrix);
    scale[1] = y;
    scale[0] = x;
    scale[3] = w;
    scale[2] = z;
}
static inline void CaedMtxScaleS(float matrix[4][4], float *scale)
{
    xglMatrixUnit(matrix);
    scale[1] = (float)D_00A56F10 * 0.055555556f + 0.0f;
    scale[0] = (float)D_00A56F10 * 0.055555556f + 0.0f;
    scale[3] = 1.0f;
    scale[2] = 1.0f;
}
#define CaedMtxScaleV(matrix, scale, x, y, z, w) \
    (CaedMtxScaleI(matrix, scale, x, y, z, w), v = scale, xglMatrixScale(matrix, matrix, v))
static inline void CaedMtxScale(float matrix[4][4], float *scale, float x, float y, float z, float w)
{
    xglMatrixUnit(matrix);
    scale[1] = y;
    scale[0] = x;
    scale[3] = w;
    scale[2] = z;
    xglMatrixScale(matrix, matrix, scale);
}
#define CaedMtxScaleK(matrix, scale, x, y, z, w) \
    (xglMatrixUnit(matrix), scale[1] = y, scale[0] = x, scale[3] = w, scale[2] = z, xglMatrixScale(matrix, matrix, scale))
#define SCL ((float)D_00A56F10 * 0.44444445f + 1.0f)
static inline void CaedMtxDraw(float matrix[4][4], void *tex, int model)
{
    nmlModelSetPlace(matrix);
    nmlModelSetTexture(tex);
    nmlModelSetToumei(1);
    nmlModelSetAlpha(0x48U);
    nmlModelEntry(model);
}
static inline void CaedMtxDrawT(void *tex, int model)
{
    nmlModelSetTexture(tex);
    nmlModelSetToumei(1);
    nmlModelSetAlpha(0x48U);
    nmlModelEntry(model);
}
static inline void CaedMtxDrawD(float matrix[4][4], int model)
{
    nmlModelSetPlace(matrix);
    nmlModelSetTexture((void *)(((D_00A56F10 & 1) * 0x3680) + 0x01960000));
    nmlModelSetToumei(1);
    nmlModelSetAlpha(0x48U);
    nmlModelEntry(model);
}

void CaedGameWinMode(s32 arg0, void *arg1, s32 arg2)
{
    float matrix[4][4];
    float scale[4];
    s32 a2 = arg2;
    CardGameWork *work = arg1;
    u16 temp_5 = (u16)work->command.step;
    s16 temp_3 = work->command.step;

    switch (temp_3) {
    case 0:
        work->command.step = (s16)(temp_5 + 1);
        D_00A56F10 = 0x19;
        return;
    case 1: {
        unsigned char *tp = &D_00A56F10;

        if (*tp != 0) {
            *tp = *tp - 1;
            return;
        }
        if (work->command.slideDirection != 0) {
            work->command.slideDirection = 0;
            work->command.slideTimer = 10;
        }
        work->mess = 0;
        work->interruptMess = 0;
        work->permanentMess = 0;
        work->player.hand.unmodeled_04[0] = CardPlayYamaCnt(&work->player.hand);
        work->enemy.hand.unmodeled_04[0] = CardPlayYamaCnt(&work->enemy.hand);
        CGPRefreshFieldHP(work);
        *tp = 0x11;
        switch (arg0) {
        case 0:
            if (a2 == 5) {
                CardPlaySound4();
            } else {
                CardPlaySound3();
            }
            work->command.step = 2;
            break;
        case 1:
            CardPlaySound3();
            work->command.step = 10;
            break;
        case 2:
            CardPlaySound3();
            work->command.step = 0x14;
            break;
        case 3:
            work->command.step = 0x1E;
            break;
        default:
            work->command.step = 0x28;
            break;
        }
        return;
    }
    case 2:
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = (s16)(temp_5 + 1);
        return;
    case 3:
        CaedMtxScale(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951E60);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    case 4: {
        float *v;
        CaedMtxScaleV(matrix, scale, 1.0f, 1.0f, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951E60);
        CaedMtxScaleV(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950F30);
        if (D_00A56F10 == 0) {
            D_00A56F10 = 0x11;
            work->command.step = work->command.step + 1;
            return;
        }
        D_00A56F10 = D_00A56F10 - 1;
        return;
    }
    case 5:
        CaedMtxScaleK(matrix, scale, 1.0f, 1.0f, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951E60);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950F30);
        return;
    case 10:
        CaedMtxScale(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951E60);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    case 11: {
        float *v;
        CaedMtxScaleV(matrix, scale, 0.9f, 0.9f, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951E60);
        CaedMtxScaleV(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950510);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    }
    case 12:
        CaedMtxScaleK(matrix, scale, 0.9f, 0.9f, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951E60);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950510);
        return;
    case 20:
        CaedMtxScale(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951E60);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    case 21: {
        float *v;
        CaedMtxScaleV(matrix, scale, 0.9f, 0.9f, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951E60);
        CaedMtxScaleV(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950A20);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    }
    case 22:
        CaedMtxScaleK(matrix, scale, 0.9f, 0.9f, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951E60);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950A20);
        return;
    case 30:
        CaedMtxScale(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950000);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    case 31: {
        float *v;
        CaedMtxScaleV(matrix, scale, 0.9f, 0.9f, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950000);
        CaedMtxScaleV(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951440);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    }
    case 32: {
        float *v;
        CaedMtxScaleV(matrix, scale, 0.9f, 0.9f, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950000);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951440);
        CaedMtxScaleV(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951950);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    }
    case 33:
        CaedMtxScaleK(matrix, scale, 0.9f, 0.9f, 1.0f, 1.0f);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01950000);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951440);
        CaedMtxDraw(matrix, (void *)0x01952D90, 0x01951950);
        return;
    case 40:
        CaedMtxScale(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        CaedMtxDrawD(matrix, 0x01952880);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    case 41:
        CaedMtxScaleK(matrix, scale, 1.0f, 1.0f, 1.0f, 1.0f);
        CaedMtxDrawD(matrix, 0x01952880);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    case 42: {
        float *v;
        CaedMtxScaleV(matrix, scale, SCL, SCL, 1.0f, 1.0f);
        nmlModelSetPlace(matrix);
        CaedMtxDrawT((void *)(((D_00A56F10 & 1) * 0x3680) + 0x01960000), 0x01952370);
        CaedMtxScaleS(matrix, scale);
        v = scale;
        xglMatrixScale(matrix, matrix, v);
        nmlModelSetPlace(matrix);
        CaedMtxDrawT((void *)(((D_00A56F10 & 1) * 0x3680) + 0x01960000), 0x01952880);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
            return;
        }
        D_00A56F10 = 0x11;
        work->command.step = work->command.step + 1;
        return;
    }
    case 43:
        CaedMtxScaleK(matrix, scale, 1.0f, 1.0f, 1.0f, 1.0f);
        CaedMtxDrawD(matrix, 0x01952370);
        if (D_00A56F10 != 0) {
            D_00A56F10 = D_00A56F10 - 1;
        } else {
            D_00A56F10 = 0x11;
        }
        return;
    }
}

extern char D_00A4C788[];

void CMTSetFontColor(s32 count, s32 player)
{
    if (count == player) {
        xglFontPrint(0, 0, 0, D_00A4C788);
    } else if (count < player) {
        xglFontPrint(0, 0, 0, D_00A4C788);
    } else {
        xglFontPrint(0, 0, 0, D_00A4C788);
    }
}

static inline void CardHideModels(CardGameWork *work) {
    int model;

    for (model = 15; model >= 0; model--)
        work->models[model].visible = 0;
}

static inline void CardLoadBattleWindows(void) {
    CardFread(0x01930000, D_00A4C5A0);
    CardFread(0x01931800, D_00A4C5B8);
    CardFread(0x01933000, D_00A4C5D0);
    CardFread(0x01934800, D_00A4C5E8);
    CardFread(0x01936000, D_00A4C600);
    CardFread(0x01937800, D_00A4C618);
}

s32 CardMainProc(CardGameWork *work) {
    float matrix[4][4];
    float placement[4][4];
    float position[4];
    StudioLight *studioLight;
    int i;
    int win;
    float pulse;

    work->command.input[0].held = PadData[0].prefix.half_28;
    work->command.input[0].pressed = PadData[0].prefix.half_2a;
    work->command.input[0].directions = PadData[0].directions;
    work->command.input[0].repeat = PadData[0].prefix.half_2a;
    work->command.input[1].held = PadData[0].prefix.half_28;
    work->command.input[1].pressed = PadData[0].prefix.half_2a;
    work->command.input[1].directions = PadData[0].directions;
    work->command.input[1].repeat = PadData[0].prefix.half_2a;
    if (work->fase == 9) {
        work->command.input[1].held = PadData[1].prefix.half_28;
        work->command.input[1].pressed = PadData[1].prefix.half_2a;
        work->command.input[1].directions = PadData[1].directions;
        work->command.input[1].repeat = PadData[1].prefix.half_2a;
    }

    xglStudioGetLight(&studioLight);
    xglLightIntensityAmbient(studioLight, work->modelColor);

    CCtimer_5 = (CCtimer_5 + 1) & 0x1F;
    pulse = xglSin(CCtimer_5 * 11.25f / 180.0f * 3.1415927f) * 0.5f + 1.0f;
    CCColor[3][0] = CCColor[3][1] = CCColor[3][2] = pulse;
    if (CCtimer_5 & 1)
        KKColor[3][0] = KKColor[3][1] = KKColor[3][2] = 1.5f;
    else
        KKColor[3][0] = KKColor[3][1] = KKColor[3][2] = 0.75f;

    if (work->helpState) {
        xglFontDebugPrintf(0x54, 0, "Help");
        CardHelpProc(work);
    } else {
        switch (work->fase) {
        case 0:
            CardFadeOut(work);
            if (work->fadeOutTimer == 0) {
                xglSoundLoadRequestSmd("Umnmode", (void *) 0x01900000);
                work->fase = 1;
                CardTitleInit(work);
                work->fadeOutTimer = 10;
            }
            break;

        case 1: {
            int previousFase;

                xglFontDebugPrintf(0x54, 0, "Menu");
                previousFase = work->fase;
                CardFadeIn(work);
                if (SubNo_2 < 0) {
                    if (PadData[0].prefix.half_2a & 0x1000) {
                        MenuNo_1--;
                        xglSoundEffectNormalID(3, 0);
                    } else if (PadData[0].prefix.half_2a & 0x4000) {
                        MenuNo_1++;
                        xglSoundEffectNormalID(3, 0);
                    }
                    if (MenuNo_1 < 0)
                        MenuNo_1 = 3;
                    if (MenuNo_1 >= 4)
                        MenuNo_1 = 0;
                    if (PadData[0].prefix.half_2a & 0x20) {
                        switch (MenuNo_1) {
                        case 0:
                        case 1:
                        case 2:
                            xglSoundEffectNormalID(1, 0);
                            SubNo_2 = 0;
                            break;
                        case 3:
                            xglSoundEffectNormalID(1, 0);
                            return 0;
                        case 4:
                            work->fase = 5;
                            break;
                        }
                    }
                } else {
                    if (PadData[0].prefix.half_2a & 0x1000) {
                        SubNo_2--;
                        xglSoundEffectNormalID(3, 0);
                    } else if (PadData[0].prefix.half_2a & 0x4000) {
                        SubNo_2++;
                        xglSoundEffectNormalID(3, 0);
                    }
                    if (MenuNo_1 == 0) {
                        if (SubNo_2 < 0)
                            SubNo_2 = 3;
                        if (SubNo_2 >= 4)
                            SubNo_2 = 0;
                    } else if (MenuNo_1 == 1) {
                        if (SubNo_2 < 0)
                            SubNo_2 = 2;
                        if (SubNo_2 >= 3)
                            SubNo_2 = 0;
                    } else {
                        if (SubNo_2 < 0)
                            SubNo_2 = 1;
                        if (SubNo_2 >= 2)
                            SubNo_2 = 0;
                    }
                    if (PadData[0].prefix.half_2a & 0x40) {
                        xglSoundEffectNormalID(2, 0);
                        SubNo_2 = -1;
                    } else if (PadData[0].prefix.half_2a & 0x20) {
                        xglSoundEffectNormalID(1, 0);
                        if (MenuNo_1 == 0) {
                            switch (SubNo_2) {
                            case 0:
                                work->fase = 11;
                                break;
                            case 1:
                                work->fase = 10;
                                break;
                            case 2:
                                work->fase = 9;
                                break;
                            case 3:
                                work->save->background++;
                                if (work->save->background >= 6)
                                    work->save->background = 0;
                                CMIBGInit(work);
                                break;
                            default:
                                /* Toggles the bit by adding it, as the original does. */
                                work->save->flags = (work->save->flags & ~CARD_SAVE_LARGE_CARDS) |
                                                    ((work->save->flags + CARD_SAVE_LARGE_CARDS) & CARD_SAVE_LARGE_CARDS);
                                xglSoundEffectNormalID(1, 0);
                                if (work->save->flags & CARD_SAVE_LARGE_CARDS) {
                                    work->models[5].model = (void *) 0x01E20000;
                                    work->models[5].texture = (void *) 0x01F70200;
                                    work->models[7].model = (void *) 0x01E20000;
                                    work->models[7].texture = (void *) 0x01F70200;
                                } else {
                                    work->models[5].model = (void *) 0x01996000;
                                    work->models[5].texture = (void *) 0x0199A000;
                                    work->models[7].model = (void *) 0x01996000;
                                    work->models[7].texture = (void *) 0x0199A000;
                                }
                                break;
                            }
                        } else if (MenuNo_1 == 1) {
                            switch (SubNo_2) {
                            case 0:
                                work->fase = 7;
                                break;
                            case 1:
                                work->faseStep = 0;
                                work->fase = 2;
                                break;
                            default:
                                work->fase = 3;
                                break;
                            }
                        } else {
                            switch (SubNo_2) {
                            case 0:
                                work->fase = 12;
                                work->fadeOutTimer = 10;
                                work->fadeOutStep = 16;
                                work->faseStep = 0;
                                break;
                            case 1:
                                work->helpState = 48;
                                work->fadeOutTimer = 10;
                                work->fadeOutStep = 16;
                                work->faseStep = 0;
                                break;
                            }
                        }
                    }
                }

                {
                    char menuColor[] = "\v\r\0\f\x10\x10\x10";
                    char cursorColor[] = "\f(((";
                    CardFontCode5 selectColor = D_00A4C780;
                    void *model;
                    void *texture;

                    position[0] = position[1] = position[2] = 0.0f;
                    position[3] = 1.0f;
                    xglMatrixUnit(matrix);

                    xglFontPrint(0, 0, 0xFFF0, menuColor);
                    if (SubNo_2 >= 0)
                        xglFontPrint(0, 0, 0xFFF0, cursorColor);
                    if (MenuNo_1 == 0) {
                        nmlModelSetLight(asColor, asDir);
                        xglFontPrint(0, 0, 0xFFF0, selectColor.text);
                    }
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0xDE, 0x8C, 0xFFF0, " Game");
                    position[1] = 0.6787f;
                    xglMatrixTrans(placement, matrix, position);
                    nmlModelSetPlace(placement);
                    nmlModelSetTexture((void *) 0x0170F600);
                    nmlModelEntryCard((void *) 0x0170B600);

                    xglFontPrint(0, 0, 0xFFF0, menuColor);
                    if (SubNo_2 >= 0)
                        xglFontPrint(0, 0, 0xFFF0, cursorColor);
                    if (MenuNo_1 == 1) {
                        nmlModelSetLight(asColor, asDir);
                        xglFontPrint(0, 0, 0xFFF0, selectColor.text);
                    }
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0xDE, 0xB1, 0xFFF0, " Card");
                    position[1] = 0.33f;
                    xglMatrixTrans(placement, matrix, position);
                    nmlModelSetPlace(placement);
                    nmlModelSetTexture((void *) 0x0170F600);
                    nmlModelEntryCard((void *) 0x0170B600);

                    xglFontPrint(0, 0, 0xFFF0, menuColor);
                    if (SubNo_2 >= 0)
                        xglFontPrint(0, 0, 0xFFF0, cursorColor);
                    if (MenuNo_1 == 2) {
                        xglFontPrint(0, 0, 0xFFF0, selectColor.text);
                        nmlModelSetLight(asColor, asDir);
                    }
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0xDE, 0xD6, 0xFFF0, D_00A4C790);
                    position[1] = -0.0202f;
                    xglMatrixTrans(placement, matrix, position);
                    nmlModelSetPlace(placement);
                    nmlModelSetTexture((void *) 0x0170F600);
                    nmlModelEntryCard((void *) 0x0170B600);

                    xglFontPrint(0, 0, 0xFFF0, menuColor);
                    if (SubNo_2 >= 0)
                        xglFontPrint(0, 0, 0xFFF0, cursorColor);
                    if (MenuNo_1 == 3) {
                        xglFontPrint(0, 0, 0xFFF0, selectColor.text);
                        nmlModelSetLight(asColor, asDir);
                    }
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0xDE, 0xFB, 0xFFF0, " Exit");
                    position[1] = -0.3692f;
                    xglMatrixTrans(placement, matrix, position);
                    nmlModelSetPlace(placement);
                    nmlModelSetTexture((void *) 0x0170F600);
                    nmlModelEntryCard((void *) 0x0170B600);

                    if (SubNo_2 >= 0) {
                        CardFontCode8 itemColor = D_00A4C7C8;
                        CardFontCode5 itemSelectColor = D_00A4C7D0;

                        position[0] = 1.8374f;
                        if (MenuNo_1 == 0) {
                            xglFontPrint(0, 0, 0xFFF0, itemColor.text);
                            if (SubNo_2 == 0) {
                                xglFontPrint(0, 0, 0xFFF0, itemSelectColor.text);
                                nmlModelSetLight(asColor, asDir);
                            }
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x168, 0x7A, 0xFFF0, "Exhibition");
                            position[1] = 0.8521f;
                            xglMatrixTrans(placement, matrix, position);
                            nmlModelSetPlace(placement);
                            nmlModelSetTexture((void *) 0x0170F600);
                            nmlModelEntryCard((void *) 0x0170D600);

                            xglFontPrint(0, 0, 0xFFF0, itemColor.text);
                            if (SubNo_2 == 1) {
                                xglFontPrint(0, 0, 0xFFF0, itemSelectColor.text);
                                nmlModelSetLight(asColor, asDir);
                            }
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x168, 0x9E, 0xFFF0, "Tournament");
                            position[1] = 0.5015f;
                            xglMatrixTrans(placement, matrix, position);
                            nmlModelSetPlace(placement);
                            nmlModelSetTexture((void *) 0x0170F600);
                            nmlModelEntryCard((void *) 0x0170D600);

                            xglFontPrint(0, 0, 0xFFF0, itemColor.text);
                            if (SubNo_2 == 2) {
                                xglFontPrint(0, 0, 0xFFF0, itemSelectColor.text);
                                nmlModelSetLight(asColor, asDir);
                            }
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x168, 0xC2, 0xFFF0, "2P Battle");
                            position[1] = 0.1509f;
                            xglMatrixTrans(placement, matrix, position);
                            nmlModelSetPlace(placement);
                            nmlModelSetTexture((void *) 0x0170F600);
                            nmlModelEntryCard((void *) 0x0170D600);

                            xglFontPrint(0, 0, 0xFFF0, itemColor.text);
                            if (SubNo_2 == 3) {
                                xglFontPrint(0, 0, 0xFFF0, itemSelectColor.text);
                                nmlModelSetLight(asColor, asDir);
                            }
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x168, 0xE8, 0xFFF0, "Change BG");
                            position[1] = -0.1976f;
                            xglMatrixTrans(placement, matrix, position);
                            nmlModelSetPlace(placement);
                            nmlModelSetTexture((void *) 0x0170F600);
                            nmlModelEntryCard((void *) 0x0170D600);
                        } else if (MenuNo_1 == 1) {
                            xglFontPrint(0, 0, 0xFFF0, itemColor.text);
                            if (SubNo_2 == 0) {
                                xglFontPrint(0, 0, 0xFFF0, itemSelectColor.text);
                                nmlModelSetLight(asColor, asDir);
                            }
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x168, 0xC2, 0xFFF0, "Create deck");
                            position[1] = 0.1509f;
                            xglMatrixTrans(placement, matrix, position);
                            nmlModelSetPlace(placement);
                            nmlModelSetTexture((void *) 0x0170F600);
                            nmlModelEntryCard((void *) 0x0170D600);

                            xglFontPrint(0, 0, 0xFFF0, itemColor.text);
                            if (SubNo_2 == 1) {
                                xglFontPrint(0, 0, 0xFFF0, itemSelectColor.text);
                                nmlModelSetLight(asColor, asDir);
                            }
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x168, 0xE8, 0xFFF0, "Open card");
                            position[1] = -0.1976f;
                            xglMatrixTrans(placement, matrix, position);
                            nmlModelSetPlace(placement);
                            nmlModelSetTexture((void *) 0x0170F600);
                            nmlModelEntryCard((void *) 0x0170D600);

                            xglFontPrint(0, 0, 0xFFF0, itemColor.text);
                            if (SubNo_2 == 2) {
                                xglFontPrint(0, 0, 0xFFF0, itemSelectColor.text);
                                nmlModelSetLight(asColor, asDir);
                            }
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x168, 0x10D, 0xFFF0, "View cards");
                            position[1] = -0.5478f;
                            xglMatrixTrans(placement, matrix, position);
                            nmlModelSetPlace(placement);
                            nmlModelSetTexture((void *) 0x0170F600);
                            nmlModelEntryCard((void *) 0x0170D600);
                        } else {
                            xglFontPrint(0, 0, 0xFFF0, itemColor.text);
                            if (SubNo_2 == 0) {
                                xglFontPrint(0, 0, 0xFFF0, itemSelectColor.text);
                                nmlModelSetLight(asColor, asDir);
                            }
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x168, 0xE8, 0xFFF0, "Tutorial");
                            position[1] = -0.1976f;
                            xglMatrixTrans(placement, matrix, position);
                            nmlModelSetPlace(placement);
                            nmlModelSetTexture((void *) 0x0170F600);
                            nmlModelEntryCard((void *) 0x0170D600);

                            xglFontPrint(0, 0, 0xFFF0, itemColor.text);
                            if (SubNo_2 == 1) {
                                xglFontPrint(0, 0, 0xFFF0, itemSelectColor.text);
                                nmlModelSetLight(asColor, asDir);
                            }
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x168, 0x10D, 0xFFF0, "Help");
                            position[1] = -0.5478f;
                            xglMatrixTrans(placement, matrix, position);
                            nmlModelSetPlace(placement);
                            nmlModelSetTexture((void *) 0x0170F600);
                            nmlModelEntryCard((void *) 0x0170D600);
                        }
                    }

                    position[0] = -1.8833f;
                    model = TitleMdlLst[MenuNo_1 * 2];
                    position[1] = 0.1576f;
                    texture = TitleMdlLst[MenuNo_1 * 2 + 1];
                    if (model != 0) {
                        xglMatrixTrans(placement, matrix, position);
                        nmlModelSetPlace(placement);
                        nmlModelSetTexture(texture);
                        nmlModelEntryCard(model);
                    }
                }
                xglFontPrint(0, 0, 0xFFF0, D_00A4C868);
                xglFontPrint(0, 0, 0, D_00A4C788);
                {
                    char *guide;

                    if (SubNo_2 < 0)
                        guide = CardPlayTMessList[50 + MenuNo_1];
                    else
                        guide = CardPlayTMessList[55 + MenuNo_1 * 5 + SubNo_2];
                    CardDispTxtSub(0x18, 0x138, guide, 0x17);
                }
                xglFontPrint(0xD8, 0x90, 0xFFF0, D_00A4C870);
                if (previousFase != work->fase) {
                    D_00A56F18.mode = 2;
                    ModeExit_4 = 0;
                    xglSoundEffectNormalID(1, 0);
                    switch (work->fase) {
                    case 0:
                        break;
                    case 3:
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->faseStep = 0;
                        work->command.step = 0;
                        break;
                    case 7:
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->faseStep = 0;
                        work->command.step = 0;
                        break;
                    case 9:
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->faseStep = 0;
                        work->command.step = 0;
                        break;
                    case 10:
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->faseStep = 0;
                        work->command.step = 0;
                        break;
                    case 11:
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->faseStep = 0;
                        work->command.step = 0;
                        break;
                    }
                }
                break;
        }

        case 2:
            CardOpenProc(work, &ModeExit_4);
            break;

        case 3:
            switch (work->faseStep) {
            case 0:
                CardFadeOut(work);
                if (work->fadeOutTimer == 0) {
                    work->fadeOutTimer = 10;
                    work->command.step = 0;
                    work->fadeOutStep = 16;
                    work->faseStep++;
                    CardHideModels(work);
                    work->models[1].visible = 1;
                    D_00A56F18.baseColor = 7;
                    D_00A56F18.cursorColor = 15;
                    D_00A56F18.x = 316;
                    D_00A56F18.y = 60;
                    D_00A56F18.lineHeight = 16;
                    D_00A56F18.rows = 9;
                    D_00A56F18.active = 0;
                    D_00A56F18.cursor = 0;
                    D_00A56F18.top = 0;
                    D_00A56F18.count = 0;
                }
                break;
            case 1:
                CardFadeIn(work);
                if (work->fadeOutTimer == 0)
                    work->faseStep++;
                break;
            default:
                CardViewProc(work, &D_00A56F18, &ModeExit_4);
                break;
            }
            break;

        case 9: {
            int previous;
            int result;
            int last;
            char *title;
            char *enemyTitle;
            int exiting;

                exiting = 0;
                switch (work->faseStep) {
                case 0:
                    CardFadeOut(work);
                    if (work->fadeOutTimer == 0) {
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->command.step = 0;
                        work->faseStep++;
                        for (i = 0; i < 16; i++)
                            work->models[i].visible = 0;
                        work->models[2].visible = 1;
                        CardFread(0x01900000, "deckmake\\type1_B.lex");
                        CardFread(0x01900400, "deckmake\\type1_E.lex");
                        CardFread(0x01900800, "deckmake\\type1_S.lex");
                        CardFread(0x01900C00, "deckmake\\type2_0.lex");
                        CardFread(0x01901000, "deckmake\\type2_1.lex");
                        CardFread(0x01901400, "deckmake\\type2_2.lex");
                        CardFread(0x01901800, "deckmake\\type2_3.lex");
                        CardFread(0x01901C00, "deckmake\\type2_4.lex");
                        CardFread(0x01902000, "deckmake\\type2_5.lex");
                        CardFread(0x01902400, "deckmake\\type2_6.lex");
                        CardFread(0x01908000, "deckmake\\type1_B.xtx");
                        CardFread(0x01918100, "deckmake\\type1_E.xtx");
                        CardFread(0x01928200, "deckmake\\type1_S.xtx");
                        CardFread(0x01938300, "deckmake\\type2_0.xtx");
                        D_00A56F18.baseColor = 7;
                        D_00A56F18.cursorColor = 15;
                        D_00A56F18.cursor = 0;
                        D_00A57188.mode = 0;
                        D_00A57188.cursor = 0;
                        D_00A56F18.active = 1;
                        D_00A56F18.top = 0;
                        D_00A56F18.rows = 9;
                        D_00A56F18.lineHeight = 16;
                        D_00A57188.lineHeight = 16;
                        D_00A57188.top = 0;
                        D_00A56F18.x = 12;
                        D_00A56F18.y = 64;
                        D_00A56F18.count = 0;
                        D_00A57188.x = 312;
                        D_00A57188.y = 60;
                        D_00A57188.columns = 1;
                        D_00A57188.rows = 9;
                        previous = -1;
                        last = -1;
                        for (i = 0; i < 40; i++) {
                            if (previous != work->save->deck[i]) {
                                last++;
                                D_00A57188.entries[last].cardId = work->save->deck[i];
                                D_00A57188.entries[last].count = 1;
                            } else {
                                D_00A57188.entries[last].count++;
                            }
                            previous = work->save->deck[i];
                        }
                        D_00A57188.count = last + 1;
                    }
                    break;
                case 1:
                    CardFadeIn(work);
                    title = P1DeckLoadStr;
                    if (ModeExit_4) {
                        title = 0;
                        xglFontPrint(0x18, 0x138, 0xFFF0, ModeEndMess0);
                        xglFontPrint(0xE0, 0x168, 0xFFF0, ModeEndMess1);
                        exiting = 1;
                        if (PadData[0].prefix.half_2a & 0x20) {
                            work->fadeOutTimer = 10;
                            work->fadeOutStep = 16;
                            work->fase = 0;
                            ModeExit_4 = 0;
                        }
                        if (PadData[0].prefix.half_2a & 0x840) {
                            xglSoundEffectNormalID(2, 0);
                            ModeExit_4 = 0;
                        }
                    } else if (work->command.input[0].pressed & 0x800) {
                        xglSoundEffectNormalID(1, 0);
                        ModeExit_4 = 1;
                    }
                    result = CardLoadDeckProc(exiting, &D_00A56F18, &D_00A57188, work, title);
                    if (result != 0) {
                        if (result >= 0) {
                            CardCopyRam2Deck(work->playerDeck, work, result - 1);
                            work->command.step = 0;
                            work->faseStep++;
                        } else {
                            ModeExit_4 = 1;
                        }
                    }
                    break;
                case 2:
                    enemyTitle = P2DeckLoadStr;
                    if (ModeExit_4) {
                        enemyTitle = 0;
                        xglFontPrint(0x18, 0x138, 0xFFF0, ModeEndMess0);
                        xglFontPrint(0xE0, 0x168, 0xFFF0, ModeEndMess1);
                        exiting = 1;
                        if (PadData[0].prefix.half_2a & 0x20) {
                            work->fadeOutTimer = 10;
                            work->fadeOutStep = 16;
                            work->fase = 0;
                            ModeExit_4 = 0;
                        }
                        if (PadData[0].prefix.half_2a & 0x840) {
                            xglSoundEffectNormalID(2, 0);
                            ModeExit_4 = 0;
                        }
                    } else if (work->command.input[0].pressed & 0x800) {
                        xglSoundEffectNormalID(1, 0);
                        ModeExit_4 = 1;
                    }
                    result = CardLoadDeckProc(exiting, &D_00A56F18, &D_00A57188, work, enemyTitle);
                    if (result != 0) {
                        if (result < 0) {
                            work->command.step = 0;
                            work->faseStep--;
                        } else {
                            CardCopyRam2Deck(work->enemyDeck, work, result - 1);
                            work->command.step = 0;
                            work->fadeOutTimer = 10;
                            work->fadeOutStep = 16;
                            work->faseStep++;
                        }
                    }
                    break;
                case 3:
                    CardFadeOut(work);
                    if (work->fadeOutTimer == 0) {
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->faseStep++;
                        work->command.step = 0;
                        CardPlaySound1();
                        CardGameInit(work);
                        CardPlayInitWork(&work->player, work->playerDeck);
                        CardPlayInitWork(&work->enemy, work->enemyDeck);
                    }
                    break;
                case 4:
                    CardFadeIn(work);
                    if (CardGameProc(0, &D_00A573F8, work, 0) != 0) {
                        work->resultWait = 90;
                        work->faseStep++;
                    }
                    break;
                case 5:
                    CardGameProc(0, &D_00A573F8, work, 0);
                    if (work->resultWait <= 0 &&
                        ((work->command.input[0].pressed & 0xF0) ||
                         (work->command.input[1].pressed & 0xF0))) {
                        xglSoundEffectNormalID(1, 0);
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->fase = 0;
                        ModeExit_4 = 0;
                    }
                    break;
                }
                break;
        }

        case 10: {
            int line;
            int pack;
            int outcome;
            int pick;
            int previous;
            int result;
            int count;
            int last;
            char *title;
            int exiting;

                exiting = 0;
                switch (work->faseStep) {
                case 0:
                    D_00A57730[3] = 0;
                    D_00A57730[2] = 0;
                    D_00A57730[1] = 0;
                    D_00A57730[0] = 0;
                    CardFadeOut(work);
                    if (work->fadeOutTimer == 0) {
                        D_00A57728 = 0;
                        work->fadeOutTimer = 10;
                        work->faseStep++;
                        work->fadeOutStep = 16;
                        for (i = 0; i < 16; i++)
                            work->models[i].visible = 0;
                        work->models[2].visible = 1;
                        CardLoadBattleWindows();
                        D_00A56F18.baseColor = 7;
                        D_00A56F18.cursorColor = 15;
                        D_00A56F18.cursor = 0;
                        D_00A57188.mode = 0;
                        D_00A57188.cursor = 0;
                        D_00A56F18.active = 1;
                        D_00A56F18.top = 0;
                        D_00A56F18.rows = 9;
                        D_00A56F18.lineHeight = 16;
                        D_00A57188.lineHeight = 16;
                        D_00A57188.top = 0;
                        D_00A56F18.x = 12;
                        D_00A56F18.y = 64;
                        D_00A56F18.count = 0;
                        D_00A57188.x = 312;
                        D_00A57188.y = 60;
                        D_00A57188.columns = 1;
                        D_00A57188.rows = 9;
                        previous = -1;
                        last = -1;
                        for (i = 0; i < 40; i++) {
                            if (previous != work->save->deck[i]) {
                                last++;
                                D_00A57188.entries[last].cardId = work->save->deck[i];
                                D_00A57188.entries[last].count = 1;
                            } else {
                                D_00A57188.entries[last].count++;
                            }
                            previous = work->save->deck[i];
                        }
                        D_00A57188.count = last + 1;
                    }
                    break;
                case 1:
                    CardFadeIn(work);
                    if (work->fadeOutTimer == 0)
                        work->faseStep++;
                    break;
                case 2:
                    title = P1DeckLoadStr;
                    if (ModeExit_4) {
                        title = 0;
                        xglFontPrint(0x18, 0x138, 0xFFF0, ModeEndMess0);
                        xglFontPrint(0xE0, 0x180, 0xFFF0, ModeEndMess1);
                        exiting = 1;
                        if (PadData[0].prefix.half_2a & 0x20) {
                            work->fadeOutTimer = 10;
                            work->fadeOutStep = 16;
                            work->fase = 0;
                            ModeExit_4 = 0;
                        }
                        if (PadData[0].prefix.half_2a & 0x840) {
                            xglSoundEffectNormalID(2, 0);
                            ModeExit_4 = 0;
                        }
                    } else if (work->command.input[0].pressed & 0x800) {
                        xglSoundEffectNormalID(1, 0);
                        ModeExit_4 = 1;
                    }
                    result = CardLoadDeckProc(exiting, &D_00A56F18, &D_00A57188, work, title);
                    if (result != 0) {
                        if (result >= 0) {
                            CardCopyRam2Deck(work->playerDeck, work, result - 1);
                            work->command.step = 0;
                            work->faseStep++;
                        } else {
                            ModeExit_4 = 1;
                        }
                    }
                    break;
                case 3:
                    count = 40;
                    if (xglFlagsGet(0x186, 1) == 0)
                        count = xglFlagsGet(0x12D, 1) != 0 ? 19 : 10;
                    for (pick = 0; pick < count; pick++)
                        D_00A57668[pick] = -1;
                    for (pick = 0; pick < count;) {
                        int *slot = &D_00A57668[xglSRand() % count];

                        if (*slot < 0) {
                            *slot = pick;
                            pick++;
                        }
                    }
                    D_00A57188.count = 5;
                    work->faseStep++;
                    /* fall through */
                case 4:
                    if (ModeExit_4) {
                        xglFontPrint(0, 0, 0, D_00A4C788);
                        xglFontPrint(0x18, 0x138, 0xFFF0, "\r\0Exit game?");
                        exiting = 1;
                        switch (D_00A57728) {
                        case 1:
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x18, 0x150, 0xFFF0, "The prize is 1 Card Pack if you stop here.");
                            break;
                        case 2:
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x18, 0x150, 0xFFF0, "The prize is 2 Card Packs if you stop here.");
                            break;
                        case 3:
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x18, 0x150, 0xFFF0, "The prize is 3 Card Packs if you stop here.");
                            break;
                        case 4:
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x18, 0x150, 0xFFF0, "The prize is 4 Card Packs if you stop here.");
                            break;
                        }
                        xglFontPrint(0xE0, 0x180, 0xFFF0, ModeEndMess1);
                        if (work->command.input[0].pressed & 0x20) {
                            CardMainItemGetSub(D_00A57730, D_00A57728);
                            work->faseStep = 15;
                        } else if (work->command.input[0].pressed & 0x840) {
                            xglSoundEffectNormalID(2, 0);
                            ModeExit_4 = 0;
                        }
                    } else {
                        if (work->command.input[0].pressed & 0x800) {
                            xglSoundEffectNormalID(1, 0);
                            ModeExit_4 = 1;
                        }
                        if (work->command.input[0].pressed & 0x20) {
                            work->faseStep++;
                            xglSoundEffectNormalID(1, 0);
                        } else if (work->command.input[0].pressed & 0x40) {
                            if (D_00A57728 != 0) {
                                xglSoundEffectNormalID(5, 0);
                                ModeExit_4 = 1;
                            } else {
                                work->faseStep--;
                                xglSoundEffectNormalID(2, 0);
                            }
                        }
                        if (D_00A57728 == 0) {
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x18, 0x138, 0xFFF0, "\v\x19\x03\r\0Select an opponent.");
                            xglFontPrint(0x18, 0x150, 0xFFF0, "\v\x19\x03\r\0Is this ok?");
                            xglFontPrint(0xE0, 0x168, 0xFFF0, ModeContMess1);
                        } else {
                            switch (D_00A57728) {
                            case 1:
                                xglFontPrint(0, 0, 0, D_00A4C788);
                                xglFontPrint(0x18, 0x138, 0xFFF0, "\v\x19\x03\r\0Entering the second round.");
                                break;
                            case 2:
                                xglFontPrint(0, 0, 0, D_00A4C788);
                                xglFontPrint(0x18, 0x138, 0xFFF0, "\v\x19\x03\r\0Entering the third round.");
                                break;
                            case 3:
                                xglFontPrint(0, 0, 0, D_00A4C788);
                                xglFontPrint(0x18, 0x138, 0xFFF0, "\v\x19\x03\r\0Entering the fourth round.");
                                break;
                            case 4:
                                xglFontPrint(0, 0, 0, D_00A4C788);
                                xglFontPrint(0x18, 0x138, 0xFFF0, "\v\x19\x03\r\0Entering the final round.");
                                break;
                            }
                            xglFontPrint(0xE0, 0x180, 0xFFF0, ModeContMess0);
                        }
                    }
                    CardHideModels(work);
                    work->models[15].visible = 1;
                    work->models[14].visible = 1;
                    position[1] = 1.751f;
                    position[3] = 1.0f;
                    position[0] = 0.0f;
                    position[2] = 0.0f;
                    xglMatrixUnit(placement);
                    xglMatrixTrans(matrix, placement, position);
                    nmlModelSetTexture((void *) 0x01937800);
                    nmlModelSetPlace(matrix);
                    nmlModelEntryCard((void *) 0x01930000);
                    position[1] -= 0.55f;
                    CMTSetFontColor(D_00A57728, 0);
                    xglFontPrint(0x6E, 0x25, 0xFFF0, EnemyDeckListStr[D_00A57668[0]]);
                    xglMatrixTrans(matrix, placement, position);
                    nmlModelSetTexture((void *) 0x01937800);
                    nmlModelSetPlace(matrix);
                    nmlModelEntryCard((void *) 0x01931800);
                    position[1] -= 0.55f;
                    CMTSetFontColor(D_00A57728, 1);
                    xglFontPrint(0x6E, 0x5E, 0xFFF0, EnemyDeckListStr[D_00A57668[1]]);
                    xglMatrixTrans(matrix, placement, position);
                    nmlModelSetTexture((void *) 0x01937800);
                    nmlModelSetPlace(matrix);
                    nmlModelEntryCard((void *) 0x01933000);
                    position[1] -= 0.55f;
                    CMTSetFontColor(D_00A57728, 2);
                    xglFontPrint(0x6E, 0x97, 0xFFF0, EnemyDeckListStr[D_00A57668[2]]);
                    xglMatrixTrans(matrix, placement, position);
                    nmlModelSetTexture((void *) 0x01937800);
                    nmlModelSetPlace(matrix);
                    nmlModelEntryCard((void *) 0x01934800);
                    position[1] -= 0.55f;
                    CMTSetFontColor(D_00A57728, 3);
                    xglFontPrint(0x6E, 0xD2, 0xFFF0, EnemyDeckListStr[D_00A57668[3]]);
                    xglMatrixTrans(matrix, placement, position);
                    nmlModelSetTexture((void *) 0x01937800);
                    nmlModelSetPlace(matrix);
                    nmlModelEntryCard((void *) 0x01936000);
                    position[1] -= 0.55f;
                    CMTSetFontColor(D_00A57728, 4);
                    xglFontPrint(0x6E, 0x10D, 0xFFF0, EnemyDeckListStr[D_00A57668[4]]);
                    work->fadeOutTimer = 10;
                    work->fadeOutStep = 16;
                    break;
                case 5:
                    CardFadeOut(work);
                    if (work->fadeOutTimer == 0) {
                        work->fadeOutStep = 16;
                        work->fadeOutTimer = 10;
                        if (D_00A57728 >= 4)
                            CardPlaySound2();
                        else
                            CardPlaySound1();
                        CardGameInit(work);
                        CardPlayInitWork(&work->player, work->playerDeck);
                        CardCopyRom2Deck(work->enemyDeck, D_00A57668[D_00A57728]);
                        work->enemyDeckId = D_00A57668[D_00A57728];
                        CardPlayInitWorkFake(&work->enemy, work->enemyDeck, work->definitions);
                        work->flags |= 0x20;
                        work->faseStep++;
                    }
                    break;
                case 6:
                    CardFadeIn(work);
                    outcome = CardGameProc(0, &D_00A573F8, work, D_00A57728);
                    if (outcome != 0) {
                        if (outcome == 1) {
                            D_00A57728++;
                            work->faseStep++;
                            work->resultWait = 90;
                        } else {
                            work->resultWait = 90;
                            work->faseStep = 20;
                        }
                    }
                    break;
                case 7:
                    CardGameProc(0, &D_00A573F8, work, D_00A57728);
                    if (work->resultWait <= 0 && (work->command.input[0].pressed & 0xF0)) {
                        if (D_00A57728 == 5) {
                            work->faseStep = 10;
                            work->resultWait = 90;
                            xglSoundEffectNormalID(6, 0);
                            work->command.step = 0;
                            work->command.flowRequest = 5;
                        } else {
                            CardHideModels(work);
                            work->models[2].visible = 1;
                            xglSoundEffectNormalID(1, 0);
                            work->faseStep = 4;
                        }
                    }
                    break;
                case 10:
                    CardGameProc(0, &D_00A573F8, work, D_00A57728);
                    if (work->resultWait == 0 && (work->command.input[0].pressed & 0xF0)) {
                        D_00A57730[3] = 0;
                        D_00A57730[2] = 0;
                        D_00A57730[1] = 0;
                        D_00A57730[0] = 0;
                        for (win = 0; win < D_00A57728; win++) {
                            int roll = xglSRand();

                            if (xglFlagsGet(0x12D, 1) == 0)
                                roll = 0;
                            if (roll & 1)
                                D_00A57730[1]++;
                            else
                                D_00A57730[0]++;
                        }
                        if (!(work->save->flags & CARD_SAVE_PROMO_GIVEN)) {
                            D_00A57730[3] = 1;
                            work->save->flags |= CARD_SAVE_PROMO_GIVEN;
                        } else {
                            D_00A57730[2] = (xglSRand() & 1) + 1;
                        }
                        xglSoundEffectNormalID(1, 0);
                        work->faseStep++;
                        work->command.step = -1;
                    }
                    break;
                case 11: {
                    char packCount[3];

                    CardGameProc(0, &D_00A573F8, work, D_00A57728);
                    work->models[14].visible = 1;
                    line = 0;
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0x14, 0x138, 0xFFF0, "\v\x19\x03\r\0Congratulations. Your prize is: ");
                    if (D_00A57730[0] != 0) {
                        xglFontPrint(0, 0, 0, D_00A4C788);
                        packCount[0] = ZenkakuListA[D_00A57730[0] * 2];
                        packCount[1] = ZenkakuListA[D_00A57730[0] * 2 + 1];
                        packCount[2] = 0;
                        line = 1;
                        xglFontPrint(0x14, 0x150, 0xFFF0, "\v\x19\x03\r\0Card Pack #821");
                        xglFontPrint(0xC8, 0x150, 0xFFF0, packCount);
                    }
                    if (D_00A57730[1] != 0) {
                        packCount[0] = ZenkakuListA[D_00A57730[1] * 2];
                        packCount[1] = ZenkakuListA[D_00A57730[1] * 2 + 1];
                        packCount[2] = 0;
                        xglFontPrint(0, 0, 0, D_00A4C788);
                        xglFontPrint(0x14, line * 24 + 0x150, 0xFFF0, "\v\x19\x03\r\0Card Pack #822");
                        xglFontPrint(0xC8, line * 24 + 0x150, 0xFFF0, packCount);
                        line++;
                    }
                    if (D_00A57730[2] != 0) {
                        xglFontPrint(0, 0, 0, D_00A4C788);
                        xglFontPrint(0x14, line * 24 + 0x150, 0xFFF0, "\v\x19\x03\r\0\xA3\xB1 Promotional Card");
                        line++;
                    }
                    if (D_00A57730[3] != 0) {
                        xglFontPrint(0, 0, 0, D_00A4C788);
                        xglFontPrint(0x14, line * 24 + 0x150, 0xFFF0, "\v\x19\x03\r\0Tuned Circuit");
                    }
                    if (work->command.input[0].pressed & 0xF0) {
                        xglSoundEffectNormalID(1, 0);
                        work->faseStep = 15;
                    }
                    break;
                }
                case 15:
                    CardMainAddItemSub(work, D_00A57730);
                    xglSoundEffectNormalID(1, 0);
                    work->fadeOutTimer = 10;
                    work->fadeOutStep = 16;
                    work->fase = 0;
                    ModeExit_4 = 0;
                    break;
                case 20:
                    CardGameProc(0, &D_00A573F8, work, D_00A57728);
                    if (work->resultWait <= 0) {
                        for (pack = 0; pack < D_00A57728; pack++) {
                            int roll = xglSRand();

                            if (xglFlagsGet(0x12D, 1) == 0)
                                roll = 0;
                            if (roll & 1)
                                D_00A57730[1]++;
                            else
                                D_00A57730[0]++;
                        }
                        work->faseStep++;
                    }
                    break;
                case 21: {
                    char packCount[3];

                    line = 0;
                    CardGameProc(0, &D_00A573F8, work, D_00A57728);
                    work->models[14].visible = 1;
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0x14, 0x138, 0xFFF0, "\v\x19\x03\r\0Sorry, you lost. Your prize is: ");
                    if (D_00A57728 != 0) {
                        if (D_00A57730[0] != 0) {
                            packCount[0] = ZenkakuListA[D_00A57730[0] * 2];
                            packCount[1] = ZenkakuListA[D_00A57730[0] * 2 + 1];
                            line = 1;
                            packCount[2] = 0;
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            xglFontPrint(0x14, 0x150, 0xFFF0, "\v\x19\x03\r\0Card Pack #821");
                            xglFontPrint(0xC8, 0x150, 0xFFF0, packCount);
                        }
                        if (D_00A57730[1] != 0) {
                            xglFontPrint(0, 0, 0, D_00A4C788);
                            packCount[0] = ZenkakuListA[D_00A57730[1] * 2];
                            packCount[1] = ZenkakuListA[D_00A57730[1] * 2 + 1];
                            packCount[2] = 0;
                            xglFontPrint(0x14, line * 24 + 0x150, 0xFFF0, "\v\x19\x03\r\0Card Pack #822");
                            xglFontPrint(0xC8, line * 24 + 0x150, 0xFFF0, packCount);
                            line++;
                        }
                    } else {
                        xglFontPrint(0, 0, 0, D_00A4C788);
                    }
                    xglFontPrint(0x14, line * 24 + 0x150, 0xFFF0, "\v\x19\x03\r\0Nothing.");
                    if (work->command.input[0].pressed & 0xF0) {
                        xglSoundEffectNormalID(1, 0);
                        work->faseStep = 15;
                    }
                    break;
                }
                }
                break;
        }

        case 11: {
            int previous;
            int model;
            int result;
            int count;
            int last;
            char *title;
            int exiting;

                exiting = 0;
                switch (work->faseStep) {
                case 0:
                    CardFadeOut(work);
                    if (work->fadeOutTimer == 0) {
                        D_00A57744 = 0;
                        work->fadeOutTimer = 10;
                        work->faseStep++;
                        work->fadeOutStep = 16;
                        for (i = 0; i < 16; i++)
                            work->models[i].visible = 0;
                        work->models[2].visible = 1;
                        CardLoadBattleWindows();
                        D_00A56F18.baseColor = 7;
                        D_00A56F18.cursorColor = 15;
                        D_00A56F18.cursor = 0;
                        D_00A57188.mode = 0;
                        D_00A57188.cursor = 0;
                        D_00A56F18.active = 1;
                        D_00A56F18.top = 0;
                        D_00A56F18.rows = 9;
                        D_00A56F18.lineHeight = 16;
                        D_00A57188.lineHeight = 16;
                        D_00A57188.top = 0;
                        D_00A56F18.x = 12;
                        D_00A56F18.y = 64;
                        D_00A56F18.count = 0;
                        D_00A57188.x = 312;
                        D_00A57188.y = 60;
                        D_00A57188.columns = 1;
                        D_00A57188.rows = 9;
                        previous = -1;
                        last = -1;
                        for (i = 0; i < 40; i++) {
                            if (previous != work->save->deck[i]) {
                                last++;
                                D_00A57188.entries[last].cardId = work->save->deck[i];
                                D_00A57188.entries[last].count = 1;
                            } else {
                                D_00A57188.entries[last].count++;
                            }
                            previous = work->save->deck[i];
                        }
                        D_00A57188.count = last + 1;
                    }
                    break;
                case 1:
                    CardFadeIn(work);
                    if (work->fadeOutTimer == 0)
                        work->faseStep++;
                    break;
                case 2:
                    title = P1DeckLoadStr;
                    if (ModeExit_4) {
                        title = 0;
                        xglFontPrint(0x18, 0x138, 0xFFF0, ModeEndMess0);
                        xglFontPrint(0xE0, 0x168, 0xFFF0, ModeEndMess1);
                        exiting = 1;
                        if (PadData[0].prefix.half_2a & 0x20) {
                            work->fadeOutTimer = 10;
                            work->fadeOutStep = 16;
                            work->fase = 0;
                            ModeExit_4 = 0;
                        }
                        if (PadData[0].prefix.half_2a & 0x840) {
                            xglSoundEffectNormalID(2, 0);
                            ModeExit_4 = 0;
                        }
                    } else if (PadData[0].prefix.half_2a & 0x840) {
                        xglSoundEffectNormalID(2, 0);
                        ModeExit_4 = 1;
                    }
                    result = CardLoadDeckProc(exiting, &D_00A56F18, &D_00A57188, work, title);
                    if (result != 0) {
                        if (result < 0) {
                            ModeExit_4 = 1;
                        } else {
                            CardCopyRam2Deck(work->playerDeck, work, result - 1);
                            work->command.step = 0;
                            work->faseStep++;
                            for (model = 15; model >= 0; model--)
                                work->models[model].visible = 0;
                            work->models[15].visible = 1;
                            work->models[14].visible = 1;
                        }
                    }
                    break;
                case 3:
                    count = 40;
                    if (xglFlagsGet(0x186, 1) == 0)
                        count = xglFlagsGet(0x12D, 1) != 0 ? 19 : 10;
                    D_00A57740[0] = xglSRand() % count;
                    D_00A57188.count = 1;
                    work->faseStep++;
                    /* fall through */
                case 4:
                    if (ModeExit_4) {
                        xglFontPrint(0x18, 0x138, 0xFFF0, ModeEndMess0);
                        xglFontPrint(0xE0, 0x168, 0xFFF0, ModeEndMess1);
                        if (PadData[0].prefix.half_2a & 0x20) {
                            work->fadeOutTimer = 10;
                            work->fadeOutStep = 16;
                            work->fase = 0;
                            ModeExit_4 = 0;
                        }
                        if (PadData[0].prefix.half_2a & 0x840) {
                            xglSoundEffectNormalID(2, 0);
                            ModeExit_4 = 0;
                        }
                    } else {
                        xglFontPrint(0, 0, 0, D_00A4C788);
                        xglFontPrint(0x18, 0x138, 0xFFF0, "\v\x19\x03\r\0Select an opponent.");
                        xglFontPrint(0x18, 0x150, 0xFFF0, "\v\x19\x03\r\0Is this ok?");
                        xglFontPrint(0xE0, 0x168, 0xFFF0, ModeContMess1);
                        if (work->command.input[0].pressed & 0x20) {
                            work->faseStep++;
                            xglSoundEffectNormalID(1, 0);
                        } else if (work->command.input[0].pressed & 0x800) {
                            ModeExit_4 = 1;
                            xglSoundEffectNormalID(1, 0);
                        } else if (work->command.input[0].pressed & 0x40) {
                            work->faseStep--;
                            xglSoundEffectNormalID(2, 0);
                        }
                    }
                    xglMatrixUnit(placement);
                    position[1] = 1.751f;
                    position[3] = 1.0f;
                    position[0] = 0.0f;
                    position[2] = 0.0f;
                    xglMatrixTrans(placement, placement, position);
                    nmlModelSetTexture((void *) 0x01937800);
                    nmlModelSetPlace(placement);
                    nmlModelEntryCard((void *) 0x01930000);
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0x78, 0x25, 0xFFF0, EnemyDeckListStr[D_00A57740[0]]);
                    work->fadeOutTimer = 10;
                    work->fadeOutStep = 16;
                    break;
                case 5:
                    CardFadeOut(work);
                    if (work->fadeOutTimer == 0) {
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->faseStep++;
                        CardPlaySound1();
                        CardGameInit(work);
                        CardPlayInitWork(&work->player, work->playerDeck);
                        CardCopyRom2Deck(work->enemyDeck, D_00A57740[0]);
                        work->enemyDeckId = D_00A57740[D_00A57744];
                        CardPlayInitWorkFake(&work->enemy, work->enemyDeck, work->definitions);
                        work->enemyDeckId = D_00A57740[0];
                        work->flags |= 0x20;
                    }
                    break;
                case 6:
                    CardFadeIn(work);
                    result = CardGameProc(0, &D_00A573F8, work, 0);
                    if (result != 0) {
                        if (result == 1)
                            D_00A57744++;
                        work->resultWait = 90;
                        work->faseStep++;
                    }
                    break;
                case 7:
                    CardGameProc(0, &D_00A573F8, work, 0);
                    if (work->resultWait <= 0 && (work->command.input[0].pressed & 0xF0)) {
                        xglSoundEffectNormalID(1, 0);
                        if (D_00A57744 != 0) {
                            int roll = xglSRand();

                            if (xglFlagsGet(0x12D, 1) == 0)
                                roll = 0;
                            if (roll & 1) {
                                D_00A57748 = 2;
                                dataEvtBoxInc(0x37);
                            } else {
                                D_00A57748 = 1;
                                dataEvtBoxInc(0x36);
                            }
                            work->faseStep++;
                        } else {
                            work->fadeOutTimer = 10;
                            work->fadeOutStep = 16;
                            work->fase = 0;
                            ModeExit_4 = 0;
                        }
                    }
                    break;
                case 8:
                    CardGameProc(0, &D_00A573F8, work, 0);
                    work->models[14].visible = 1;
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0x18, 0x138, 0xFFF0, "\v\x19\x03\r\0Congratulations!");
                    if (D_00A57748 == 1)
                        xglFontPrint(0x18, 0x150, 0xFFF0, "\v\x19\x03\r\0Your prize is 1 Card Pack #821.");
                    else
                        xglFontPrint(0x18, 0x150, 0xFFF0, "\v\x19\x03\r\0Your prize is 1 Card Pack #822.");
                    if (work->command.input[0].pressed & 0xF0) {
                        xglSoundEffectNormalID(1, 0);
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->fase = 0;
                        ModeExit_4 = 0;
                    }
                    break;
                }
                break;
        }

        case 7: {
            int status;
            int entry;
            int card;
            int previous;
            int last;
            int exiting;

                exiting = 0;
                xglFontPrint(0, 0, 0, D_00A4C788);
                xglFontDebugPrintf(0x54, 0, "\v\x19\x03\r\0Deck building mode");
                switch (work->faseStep) {
                case 0:
                case 1:
                case 2:
                case 3:
                    break;
                default:
                    if (work->command.input[0].pressed & 0x800) {
                        ModeExit_4 = (ModeExit_4 + 1) & 1;
                        if (ModeExit_4)
                            xglSoundEffectNormalID(1, 0);
                        else
                            xglSoundEffectNormalID(2, 0);
                    }
                    break;
                }
                if (ModeExit_4) {
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0x18, 0x138, 0xFFF0, "\v\x19\x03\r\0Exit deck building mode?");
                    xglFontPrint(0xE0, 0x168, 0xFFF0, ModeEndMess1);
                    exiting = 1;
                    if (PadData[0].prefix.half_2a & 0x20) {
                        xglSoundEffectNormalID(1, 0);
                        work->fadeOutTimer = 10;
                        work->fadeOutStep = 16;
                        work->fase = 0;
                        ModeExit_4 = 0;
                    }
                    if (PadData[0].prefix.half_2a & 0x40) {
                        xglSoundEffectNormalID(2, 0);
                        ModeExit_4 = 0;
                    }
                } else if (Message_6 != 0) {
                    exiting = 1;
                    xglFontPrint(0x18, 0x138, 0xFFFF, "\v\r\0\f\x80@@");
                    xglFontPrint(0, 0, 0, D_00A4C788);
                    xglFontPrint(0x18, 0x138, 0xFFFF, CardErrorList[Message_6]);
                    if (work->command.input[0].pressed & 0xF0) {
                        xglSoundEffectNormalID(1, 0);
                        MessageCnt_7 = 0;
                        Message_6 = 0;
                    }
                    if (MessageCnt_7 > 0) {
                        MessageCnt_7--;
                    } else {
                        MessageCnt_7 = 0;
                        Message_6 = 0;
                    }
                }
                switch (work->faseStep) {
                case 0:
                    work->command.step = 0;
                    work->faseStep++;
                    for (entry = 0; entry < 40; entry++) {
                        D_00A57188.entries[entry].cardId = -1;
                        D_00A57188.entries[entry].count = 0;
                    }
                    work->fadeOutTimer = 10;
                    work->fadeOutStep = 16;
                    break;
                case 1:
                    CardFadeOut(work);
                    if (work->fadeOutTimer == 0)
                        work->faseStep++;
                    break;
                case 2:
                    work->fadeOutTimer = 10;
                    work->fadeOutStep = 16;
                    work->faseStep++;
                    for (i = 0; i < 16; i++)
                        work->models[i].visible = 0;
                    work->models[2].visible = 1;
                    CardFread(0x01900000, "deckmake\\type1_B.lex");
                    CardFread(0x01900400, "deckmake\\type1_E.lex");
                    CardFread(0x01900800, "deckmake\\type1_S.lex");
                    CardFread(0x01900C00, "deckmake\\type2_0.lex");
                    CardFread(0x01901000, "deckmake\\type2_1.lex");
                    CardFread(0x01901400, "deckmake\\type2_2.lex");
                    CardFread(0x01901800, "deckmake\\type2_3.lex");
                    CardFread(0x01901C00, "deckmake\\type2_4.lex");
                    CardFread(0x01902000, "deckmake\\type2_5.lex");
                    CardFread(0x01902400, "deckmake\\type2_6.lex");
                    CardFread(0x01908000, "deckmake\\type1_B.xtx");
                    CardFread(0x01918100, "deckmake\\type1_E.xtx");
                    CardFread(0x01928200, "deckmake\\type1_S.xtx");
                    CardFread(0x01938300, "deckmake\\type2_0.xtx");
                    D_00A56F18.baseColor = 7;
                    D_00A56F18.cursorColor = 15;
                    D_00A56F18.cursor = 0;
                    D_00A57188.mode = 0;
                    D_00A57188.cursor = 0;
                    D_00A56F18.active = 1;
                    D_00A56F18.y = 60;
                    D_00A56F18.top = 0;
                    D_00A56F18.rows = 9;
                    D_00A56F18.lineHeight = 16;
                    D_00A57188.lineHeight = 16;
                    D_00A57188.top = 0;
                    D_00A56F18.x = 12;
                    D_00A56F18.count = 0;
                    D_00A57188.x = 312;
                    D_00A57188.y = 60;
                    D_00A57188.columns = 1;
                    D_00A57188.rows = 9;
                    last = -1;
                    previous = -1;
                    for (i = 0; i < 40; i++) {
                        if (previous != work->save->deck[i]) {
                            last++;
                            D_00A57188.entries[last].cardId = work->save->deck[i];
                            D_00A57188.entries[last].count = 1;
                        } else {
                            D_00A57188.entries[last].count++;
                        }
                        previous = work->save->deck[i];
                    }
                    D_00A57188.count = last + 1;
                    break;
                case 3:
                    CardFadeIn(work);
                    if (work->fadeOutTimer == 0)
                        work->faseStep++;
                    break;
                case 4:
                    if (!exiting) {
                        xglFontPrint(0, 0, 0, D_00A4C788);
                        xglFontPrint(0x18, 0x138, 0xFFF0, "\v\x19\x03\r\0Load deck?");
                        xglFontPrint(0x18, 0x168, 0xFFF0,
                                     "\r\x02\f\x80@@\xA1\xFB \v\x19\x03\r\0Yes\xA1\xA1\r\x02\f@@\x80\xA1\xDF \v\x19\x03\r\0No");
                        xglFontPrint(0x78, 0x120, 0xFFF0, D_00A4C870);
                        if (work->command.input[0].pressed & 0x20) {
                            int prevId;
                            int lastEntry;

                            xglSoundEffectNormalID(1, 0);
                            prevId = -1;
                            lastEntry = -1;
                            for (card = 0; card < 40; card++) {
                                if (prevId != work->save->deck[card]) {
                                    lastEntry++;
                                    D_00A57188.entries[lastEntry].cardId = work->save->deck[card];
                                    D_00A57188.entries[lastEntry].count = 1;
                                } else {
                                    D_00A57188.entries[lastEntry].count++;
                                }
                                prevId = work->save->deck[card];
                            }
                            D_00A57188.count = lastEntry + 1;
                            work->faseStep++;
                        } else if (work->command.input[0].pressed & 0x40) {
                            xglSoundEffectNormalID(2, 0);
                            D_00A57188.count = 0;
                            work->faseStep = 10;
                        }
                    }
                    break;
                case 5:
                    status = CardLoadDeckProc(exiting, &D_00A56F18, &D_00A57188, work, 0);
                    if (status != 0) {
                        if (status < 0) {
                            work->faseStep--;
                        } else {
                            CardCopyRam2CLD(&D_00A57188, work, (status & 0x7F) - 1);
                            work->command.step = 0;
                            work->faseStep = 10;
                        }
                    }
                    break;
                case 10:
                    CardHideModels(work);
                    work->faseStep++;
                    work->models[3].visible = 1;
                    D_00A56F18.baseColor = 7;
                    D_00A56F18.cursorColor = 15;
                    D_00A56F18.active = 1;
                    D_00A56F18.lineHeight = 16;
                    D_00A56F18.cursor = 0;
                    D_00A56F18.top = 0;
                    D_00A56F18.rows = 9;
                    D_00A56F18.count = 0;
                    D_00A57188.x = 312;
                    D_00A57188.y = 60;
                    D_00A57188.lineHeight = 16;
                    D_00A57188.cursor = 0;
                    D_00A57188.columns = 1;
                    D_00A57188.top = 0;
                    D_00A573F8.cursor = 0;
                    D_00A573F8.top = 0;
                    D_00A56F18.x = 12;
                    D_00A56F18.y = 64;
                    D_00A56F18.mode = 2;
                    D_00A57188.rows = 9;
                    D_00A573F8.x = 312;
                    D_00A573F8.y = 60;
                    D_00A573F8.lineHeight = 16;
                    D_00A573F8.columns = 1;
                    D_00A573F8.rows = 9;
                    D_00A573F8.count = 0;
                    break;
                case 11:
                    status = CardMakeDeckProc(exiting, &D_00A56F18, &D_00A57188, work);
                    if (status != 0) {
                        xglSoundEffectNormalID(5, 0);
                        Message_6 = status;
                        MessageCnt_7 = 45;
                    }
                    if (work->command.input[0].pressed & 0x80) {
                        if (CardGetDeckAllCnt(&D_00A57188) == 40) {
                            xglSoundEffectNormalID(1, 0);
                            work->faseStep++;
                        } else {
                            xglSoundEffectNormalID(5, 0);
                            Message_6 = 2;
                            MessageCnt_7 = 45;
                        }
                    }
                    break;
                case 12:
                    work->command.step = 0;
                    work->faseStep++;
                    D_00A56F18.count = 5;
                    D_00A56F18.top = 0;
                    D_00A56F18.cursor = 0;
                    CardCopyRam2CLD(&D_00A573F8, work, 0);
                    CardHideModels(work);
                    work->models[2].visible = 1;
                    /* fall through */
                case 13: {
                    char saveTitle[] = "\v\x19\x03\r\0Save Mode";

                    status = CardSaveDeckProc(exiting, &D_00A56F18, &D_00A573F8, work, saveTitle);
                    if (status != 0) {
                        if (status < 0) {
                            xglSoundEffectNormalID(2, 0);
                            work->faseStep = 10;
                        } else {
                            CardCopyCLD2Ram(&D_00A57188, work, status - 1);
                            xglSoundEffectNormalID(1, 0);
                            work->fadeOutTimer = 10;
                            work->fadeOutStep = 16;
                            work->fase = 0;
                        }
                    }
                    break;
                }
                }
                break;
        }

        case 12: {
            int running = 1;

            switch (work->faseStep) {
            case 0:
                CardFadeOut(work);
                if (work->fadeOutTimer == 0)
                    work->faseStep++;
                break;
            case 1:
                work->faseStep = 2;
                work->fadeOutTimer = 10;
                work->fadeOutStep = 16;
                CardHideModels(work);
                CardTutorialInit();
                break;
            case 2:
                CardFadeIn(work);
                if (work->fadeOutTimer == 0)
                    work->faseStep++;
                break;
            case 3:
                running = CardTutorialProc(work);
                break;
            }
            if (running == 0) {
                xglSoundEffectNormalID(1, 0);
                work->faseStep = 0;
                work->fadeOutTimer = 10;
                work->fadeOutStep = 16;
                work->fase = 0;
            }
            break;
        }

        case 5: {
            int soundList[25] = {
                0x20001, 0x20002, 0x20003, 0x20004, 0x20005, 0x20006, 0x20007,
                0x20008, 0x20009, 0x2000A, 0x2000D, 0x2000E, 0x2000F, 0x2000F,
                0x2000F, 0x2000F, 0x2000F, 0x2000F, 0x2000F, 0x2000F, 0x2000F,
                0x2000F, 0x2000F, 0x2000F, 0x2000F,
            };
            char volumeText[16];

            if ((PadData[0].prefix.half_2a & 8) && vol_19 < 127) {
                vol_19 += 10;
                if (vol_19 >= 128)
                    vol_19 = 127;
            }
            if ((PadData[0].prefix.half_2a & 4) && vol_19 > 0) {
                vol_19 -= 10;
                if (vol_19 < 0)
                    vol_19 = 0;
            }
            if (ModeExit_4) {
                xglFontPrint(0xC0, 0x90, 0xFFF8, ModeEndMess2);
                xglFontPrint(0xA8, 0xC0, 0xFFF8, ModeEndMess3);
                if (PadData[0].prefix.half_2a & 0x20) {
                    work->fadeOutTimer = 10;
                    work->fadeOutStep = 16;
                    work->fase = 0;
                    ModeExit_4 = 0;
                    xglSoundEffectNormalID(1, 0);
                }
                if (PadData[0].prefix.half_2a & 0x40) {
                    ModeExit_4 = 0;
                    xglSoundEffectNormalID(2, 0);
                }
            }
            xglFontPrint(0x30, 0x60, 0xFFF0, "1P Deck");
            xglFontPrint(0x48, 0x78, 0xFFF0, "T1");
            xglFontPrint(0x90, 0x78, 0xFFF0, "T2");
            xglFontPrint(0xD8, 0x78, 0xFFF0, "T3");
            xglFontPrint(0x120, 0x78, 0xFFF0, "T4");
            xglFontPrint(0x168, 0x78, 0xFFF0, "T5");
            xglFontPrint(0x48, 0x90, 0xFFF0, "T6");
            xglFontPrint(0x90, 0x90, 0xFFF0, "T7");
            xglFontPrint(0xD8, 0x90, 0xFFF0, "T8");
            xglFontPrint(0x120, 0x90, 0xFFF0, "T9");
            xglFontPrint(0x168, 0x90, 0xFFF0, "T0");
            xglFontPrint(0x30, 0xC0, 0xFFF0, "2P deck");
            xglFontPrint(0x48, 0xD8, 0xFFF0, "T1");
            xglFontPrint(0x90, 0xD8, 0xFFF0, "T2");
            xglFontPrint(0xD8, 0xD8, 0xFFF0, "T3");
            xglFontPrint(0x120, 0xD8, 0xFFF0, "T4");
            xglFontPrint(0x168, 0xD8, 0xFFF0, "T5");
            xglFontPrint(0x48, 0xF0, 0xFFF0, "T6");
            xglFontPrint(0x90, 0xF0, 0xFFF0, "T7");
            xglFontPrint(0xD8, 0xF0, 0xFFF0, "T8");
            xglFontPrint(0x120, 0xF0, 0xFFF0, "T9");
            xglFontPrint(0x168, 0xF0, 0xFFF0, "T0");
            xglFontPrint(0x30, 0x120, 0xFFF0, "Game play");
            xglFontPrint(0x18, (s16) (SubMenu_3 * 0x60 + 0x60), 0xFFF0, ">");
            if (p0num_17 < 5)
                xglFontPrint((s16) (p0num_17 * 0x48 + 0x30), 0x78, 0xFFF0, ">");
            else
                xglFontPrint((s16) (p0num_17 * 0x48 - 0x138), 0x90, 0xFFF0, ">");
            if (p1num_18 < 5)
                xglFontPrint((s16) (p1num_18 * 0x48 + 0x30), 0xD8, 0xFFF0, ">");
            else
                xglFontPrint((s16) (p1num_18 * 0x48 - 0x138), 0xF0, 0xFFF0, ">");
            if (PadData[0].prefix.half_2a & 0x1000) {
                SubMenu_3--;
                xglSoundEffectNormalID(3, 0);
            }
            if (PadData[0].prefix.half_2a & 0x4000) {
                SubMenu_3++;
                xglSoundEffectNormalID(3, 0);
            }
            if (SubMenu_3 < 0)
                SubMenu_3 = 0;
            if (SubMenu_3 >= 3)
                SubMenu_3 = 2;
            sprintf(volumeText, "%d", vol_19);
            xglFontDebugPrintf(0x18, 0x24, volumeText);
            switch (SubMenu_3) {
            case 0:
                if (PadData[0].prefix.half_2a & 0x2000) {
                    p0num_17++;
                    xglSoundEffectNormalID(3, 0);
                }
                if (PadData[0].prefix.half_2a & 0x8000) {
                    p0num_17--;
                    xglSoundEffectNormalID(3, 0);
                }
                if (PadData[0].prefix.half_2a & 0x20)
                    xglSoundEffectParamDirect(soundList[p0num_17], vol_19, 0x40);
                if (p0num_17 < 0)
                    p0num_17 = 0;
                if (p0num_17 >= 10)
                    p0num_17 = 9;
                break;
            case 1:
                if (PadData[0].prefix.half_2a & 0x2000) {
                    p1num_18++;
                    xglSoundEffectNormalID(3, 0);
                }
                if (PadData[0].prefix.half_2a & 0x8000) {
                    p1num_18--;
                    xglSoundEffectNormalID(3, 0);
                }
                if (PadData[0].prefix.half_2a & 0x20)
                    xglSoundEffectNormalID(soundList[p1num_18 + 7], 0);
                if (p1num_18 < 0)
                    p1num_18 = 0;
                if (p1num_18 >= 10)
                    p1num_18 = 9;
                break;
            case 2:
                if (PadData[0].prefix.half_2a & 0x20) {
                    ModeExit_4 = 0;
                    work->fase = 6;
                    xglSoundEffectNormalID(1, 0);
                    CardGameInit(work);
                    switch (p0num_17) {
                    case 0:
                        CardSampleInitDeck0(work->playerDeck);
                        break;
                    case 1:
                        CardSampleInitDeck1(work->playerDeck);
                        break;
                    case 2:
                        CardSampleInitDeck2(work->playerDeck);
                        break;
                    case 3:
                        CardSampleInitDeck3(work->playerDeck);
                        break;
                    case 4:
                        CardSampleInitDeck4(work->playerDeck);
                        break;
                    case 5:
                        CardSampleInitDeck5(work->playerDeck);
                        break;
                    case 6:
                        CardSampleInitDeck6(work->playerDeck);
                        break;
                    case 7:
                        CardSampleInitDeck7(work->playerDeck);
                        break;
                    case 8:
                        CardSampleInitDeck8(work->playerDeck);
                        break;
                    case 9:
                        CardSampleInitDeck9(work->playerDeck);
                        break;
                    }
                    switch (p1num_18) {
                    case 0:
                        CardSampleInitDeck0(work->enemyDeck);
                        break;
                    case 1:
                        CardSampleInitDeck1(work->enemyDeck);
                        break;
                    case 2:
                        CardSampleInitDeck2(work->enemyDeck);
                        break;
                    case 3:
                        CardSampleInitDeck3(work->enemyDeck);
                        break;
                    case 4:
                        CardSampleInitDeck4(work->enemyDeck);
                        break;
                    case 5:
                        CardSampleInitDeck5(work->enemyDeck);
                        break;
                    case 6:
                        CardSampleInitDeck6(work->enemyDeck);
                        break;
                    case 7:
                        CardSampleInitDeck7(work->enemyDeck);
                        break;
                    case 8:
                        CardSampleInitDeck8(work->enemyDeck);
                        break;
                    case 9:
                        CardSampleInitDeck9(work->enemyDeck);
                        break;
                    }
                    CardPlayNoInitWork(&work->player, work->playerDeck);
                    CardPlayNoInitWork(&work->enemy, work->enemyDeck);
                }
                break;
            }
            break;
        }

        case 6:
            if (CardGameProc(ModeExit_4, &D_00A573F8, work, 0) != 0 &&
                (work->command.input[0].pressed & 0xF0)) {
                xglSoundEffectNormalID(1, 0);
                work->fadeOutTimer = 10;
                work->fadeOutStep = 16;
                work->fase = 0;
                ModeExit_4 = 0;
            }
            break;
        }
    }

    if (work->helpState) {
        float helpLight[4][4];

        memset(helpLight, 0, sizeof(helpLight));
        helpLight[0][3] = 1.0f;
        helpLight[1][3] = 1.0f;
        helpLight[2][3] = 1.0f;
        helpLight[3][0] = 0.3f;
        helpLight[3][1] = 0.3f;
        helpLight[3][2] = 0.3f;
        helpLight[3][3] = 1.0f;
        nmlModelSetLight(helpLight, asDir);
        nmlModelSetTexture(work->models[15].texture);
        xglMatrixUnit(placement);
        nmlModelSetPlace(placement);
        nmlModelEntryCard(work->models[15].model);
    } else {
        for (i = 0; i < 16; i++) {
            CardModelEntry *model = &work->models[i];

            if (model->visible) {
                if (i == 4)
                    nmlModelSetLight(Player1Color, asDir);
                if (i == 6)
                    nmlModelSetLight(Player2Color, asDir);
                if (i == 5 || i == 7) {
                    float cardLight[4][4];

                    memset(cardLight, 0, sizeof(cardLight));
                    cardLight[0][3] = 1.0f;
                    cardLight[1][3] = 1.0f;
                    cardLight[2][3] = 1.0f;
                    cardLight[3][0] = 0.5f;
                    cardLight[3][1] = 0.5f;
                    cardLight[3][2] = 0.5f;
                    cardLight[3][3] = 1.0f;
                    nmlModelSetLight(cardLight, asDir);
                } else if (i == 15) {
                    float dimLight[4][4];

                    memset(dimLight, 0, sizeof(dimLight));
                    dimLight[0][3] = 1.0f;
                    dimLight[1][3] = 1.0f;
                    dimLight[2][3] = 1.0f;
                    dimLight[3][0] = 0.3f;
                    dimLight[3][1] = 0.3f;
                    dimLight[3][2] = 0.3f;
                    dimLight[3][3] = 1.0f;
                    nmlModelSetLight(dimLight, asDir);
                }
                nmlModelSetTexture(model->texture);
                nmlModelSetPlace(work->models[i].matrix);
                nmlModelEntryCard(model->model);
            }
        }
    }

    if (work->helpState) {
        u16 pressed;

        if (work->menuPlayer == 0)
            pressed = work->command.input[0].pressed;
        else
            pressed = work->command.input[1].pressed;
        if (pressed & 0x900) {
            xglSoundEffectNormalID(2, 0);
            work->helpState = 0;
        }
    } else if (ModeExit_4 == 0 && work->fase != 12) {
        if (work->command.input[0].pressed & 0x100) {
            xglSoundEffectNormalID(1, 0);
            work->helpState = 1;
            work->menuPlayer = 0;
            CardHelpPageInit(work);
        } else if (work->command.input[1].pressed & 0x100) {
            xglSoundEffectNormalID(1, 0);
            work->helpState = 1;
            work->menuPlayer = 1;
            CardHelpPageInit(work);
        }
    }

    xglRenderGlobalFadeSet(0.0f, 0.0f, 0.0f, (0x80 - work->fadeOutColor[3]) * 0.0078125f);

    if (work->command.slideTimer != 0) {
        float step = 0.1f;

        work->command.slideTimer--;
        if (work->command.slideDirection)
            step = -0.1f;
        work->command.slideOffset += step;
    } else {
        work->command.slideOffset = 0.0f;
        if (work->command.slideDirection)
            work->command.slideOffset = -1.0f;
    }

    position[2] = 5.8f;
    position[3] = 1.0f;
    position[0] = 0.0f;
    position[1] = 0.0f;
    {
        float *eye = &xglStudioSelectGetActiveCamera(0)->position.x;

        /* sceVu0CopyVector: the original copies the quadword with lq/sq. */
        __asm__ __volatile__(
            "lq $2, 0(%1)\n"
            "sq $2, 0(%0)\n"
            :
            : "r" (eye), "r" (position)
            : "$2", "memory");
    }
    return 1;
}

/* Types recovered upstream (origin/master); kept for reference. */
union CardSoundPathStorage {
    char bytes[40];
    struct CardSoundPath path;
};


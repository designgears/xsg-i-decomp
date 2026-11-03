#include "common.h"
#include "shared.h"
#include "file.h"
#include "main/xgl_jpeg.h"

typedef struct FileThumbnailRequest {
    u32 source;
    u32 destination;
    u8 unmodeled_08[8];
    u16 destination_x;
    u16 destination_y;
    u16 width;
    u16 height;
    u8 unmodeled_18[8];
} FileThumbnailRequest;

/* FileWork+0x09 is the signed current-slot byte used by the JPEG routines. */
typedef struct FileWorkCurrentSlotView {
    unsigned char unmodeled_00[0x09];
    signed char current_slot;
} FileWorkCurrentSlotView;

/* Five decoder buffers are 0xe100 bytes apart; their signed object number is +1. */
typedef struct FileJpegSlotView {
    unsigned char unmodeled_00;
    signed char number;
    unsigned char unmodeled_02[0xe100 - 0x02];
} FileJpegSlotView;

typedef struct FileJpegDecodeSlotView {
    unsigned char unmodeled_00;
    signed char number;
    unsigned char unmodeled_02[0x80 - 0x02];
    unsigned int dma_qwc;
    unsigned int dma_address;
    unsigned int vif_nop;
    unsigned int vif_directhl;
    unsigned int gif_tag_low;
    unsigned int gif_tag_high;
    unsigned int gif_regs_low;
    unsigned int gif_regs_high;
    unsigned char image[0xe100 - 0xa0];
} FileJpegDecodeSlotView;

typedef struct FileObjectEntryView {
    FileJpegDecodeSlotView *jpeg;
    unsigned char thumbnail[0x1000];
    unsigned char state;
    signed char number;
    unsigned char unmodeled_1006;
    unsigned char chapter;
    unsigned char leader;
    unsigned char unmodeled_1009[3];
    int map_number;
    int party_ids[16];
    u64 play_time;
    XglClock elapsed;
} FileObjectEntryView;

extern void FileJpegDecode(int number);
extern char *name_0_0036D628[];

static void VersionUpDate(SaveDataHeader *save_data)
{
    save_data->version += 1;
    FileCheckSumGet((FileChecksumData *)save_data, (long long *)&save_data->checksum);
}

typedef struct SaveHead00 {
    u64 data[8];
} SaveHead00;
typedef struct SaveBody00 {
    u64 data[0x16360 / 8];
} SaveBody00;
typedef struct SaveData00 {
    SaveHead00 head;
    SaveBody00 body;
} SaveData00;
typedef struct SaveData01 {
    union {
        SaveHead00 head;
        struct {
            u8 pad[0x3C];
            int x3C;
            int x40;
            int x44;
        } f;
    } u;
    u64 pad48;
    SaveBody00 body;
} SaveData01;

static void VersionChange00To01(SaveDataHeader *save_data)
{
    SaveData01 tmp;

    tmp.u.f.x40 = 0;
    tmp.u.head = ((SaveData00 *)save_data)->head;
    tmp.u.f.x3C = 0;
    tmp.u.f.x44 = 0;
    tmp.body = ((SaveData00 *)save_data)->body;
    *(SaveData01 *)save_data = tmp;
    VersionUpDate(save_data);
}

void FileVersionCheck(void)
{
}

int FileObjectDataNoGet(int number, int offset)
{
    number += offset;
    if (number < 0)
        return number + 99;
    if (number < 99)
        return number;
    return number - 99;
}

void FileObjectDataClear(void)
{
    int i;

    for (i = 0; i <= 100; ++i) {
        FileObjectData[i * 0x1060 + 0x1004] = 0;
        FileObjectData[i * 0x1060 + 0x1005] = -1;
    }
    for (i = 0; i <= 4; ++i)
        FileJpegDec[i * 0xe100 + 1] = -1;
}

void FileThumbnailDecode(u32 source, u32 destination)
{
    FileThumbnailRequest request;

    memset(&request, 0, sizeof(request));
    request.source = source;
    request.destination = destination;
    request.destination_x = 0;
    request.destination_y = 0;
    request.width = 0;
    request.height = 0;
    xglJpegDecode(&request);
}

void FileCheckSumGet(FileChecksumData *source, long long *checksum)
{
    FileChecksumData local = *source;
    byte *data = local.data;
    unsigned int count = 0;
    int i = 0;

    local.fields.stored_checksum = 0;
    *checksum = 0;
    do {
        *checksum += (long long)*data * i + 0x7ca;
        ++data;
        ++i;
        ++count;
    } while (count <= 0x163af);
}

int FileCheckSumCheck(FileChecksumData *data)
{
    long long checksum;

    FileCheckSumGet(data, &checksum);
    return data->fields.stored_checksum == checksum;
}

typedef struct FileObjectEntry {
    u64 data[0x1060 / 8];
} FileObjectEntry;
typedef struct FileTime8 {
    u8 b[8];
} FileTime8;
typedef struct FileSaveSrc {
    u64 kind;
    u8 pad08[0x60];
    FileTime8 time;
} FileSaveSrc;
extern FileChecksumData *FileSaveData;
extern u8 *FileObjectDataSystem;
void PartyTimeDispChange(void *);
int PartyAllPartyGet2(int *, void *);

void FileObjectDataInput(int number, int mode)
{
    u8 *entry = FileObjectData + number * 0x1060;
    u8 *src;
    u8 *dst;
    u8 *thumb;
    int *party;
    int i;
    int hp;
    u64 tm;

    memset(entry, 0, 0x1060);
    switch (mode) {
    case 2:
        *(FileObjectEntry *)entry = ((FileObjectEntry *)FileObjectData)[99];
        break;
    case 3:
        *(FileObjectEntry *)entry = ((FileObjectEntry *)FileObjectData)[100];
        break;
    case 4:
        *(FileObjectEntry *)entry = ((FileObjectEntry *)FileObjectData)[101];
        break;
    case 1:
        if (((FileSaveSrc *)FileSaveData)->kind != 3) {
            *(FileObjectEntry *)entry = ((FileObjectEntry *)FileObjectData)[101];
        } else if (FileCheckSumCheck(FileSaveData) == 0) {
            *(FileObjectEntry *)entry = ((FileObjectEntry *)FileObjectData)[101];
        } else {
            goto load;
        }
        *(u8 *)(number + (int)FileObjectDataSystem + 0x67) = 4;
        break;
    load:
            party = (int *)(entry + 0x1010);
            entry[0x1007] = FileSaveData->data[0x10252];
            entry[0x1008] = FileSaveData->data[0x10718];
            hp = *(short *)&FileSaveData->data[0x10250];
            tm = *(u64 *)&FileSaveData->data[0x15088];
            *(FileTime8 *)(entry + 0x1058) = ((FileSaveSrc *)FileSaveData)->time;
            *(int *)(entry + 0x100C) = hp;
            *(u64 *)(entry + 0x1050) = tm;
            PartyTimeDispChange(entry + 0x1058);
            src = &FileSaveData->data[0x10078];
            memset(party, 0, 0x40);
            PartyAllPartyGet2(party, src);
            thumb = &FileSaveData->data[0x15390];
            for (i = 0, dst = entry + 4; i < 0x1000; i++)
                dst[i] = thumb[i];
        break;
    default:
        break;
    }
    entry[0x1005] = number;
}

/*
 * Frees every decoder slot whose object data number is no longer one of the
 * five numbers around the slot the file menu is showing.
 */
void FileJpegCheck(void)
{
    signed char valid_numbers[5];
    FileJpegSlotView *slot;
    int i;
    int j;

    i = -1;
    do {
        valid_numbers[i + 1] = FileObjectDataNoGet(((FileWorkCurrentSlotView *)FileWork)->current_slot, i);
        i = i + 1;
    } while (i < 4);

    for (i = 0; i < 5; ++i) {
        slot = (FileJpegSlotView *)FileJpegDec + i;
        for (j = 0; j < 5; ++j) {
            if (slot->number == valid_numbers[j])
                break;
        }
        if (j == 5)
            slot->number = -1;
    }
}

void FileJpegDecode(int number)
{
    FileObjectEntryView *entry = (FileObjectEntryView *)FileObjectData + number;
    FileJpegDecodeSlotView *jpeg = (FileJpegDecodeSlotView *)FileJpegDec;
    FileJpegDecodeSlotView *queued;
    FileJpegDecodeSlotView *slot;
    int i;

    queued = jpeg;
    for (i = 0; i < 5; ++i) {
        if (queued->number == number)
            return;
        ++queued;
    }

    slot = jpeg;
    for (i = 0; i < 5; ++i) {
        if (slot->number < 0) {
            FileThumbnailDecode((u32)entry->thumbnail, (u32)slot->image);
            slot->dma_qwc = 0;
            slot->dma_address = 0;
            slot->vif_nop = 0;
            slot->vif_directhl = 0x51000e01;
            slot->gif_tag_low = 0x8e00;
            slot->gif_tag_high = 0x08000000;
            slot->gif_regs_low = 0;
            slot->gif_regs_high = 0;
            entry->jpeg = slot;
            slot->number = number;
            return;
        }
        ++slot;
    }
}

void FileObjectJpegSet(void)
{
    int offset;
    int slot;

    FileJpegCheck();
    offset = -1;
    do {
        slot = FileObjectDataNoGet(((FileWorkCurrentSlotView *)FileWork)->current_slot, offset);
        offset += 1;
        FileJpegDecode(slot);
    } while (offset < 4);
}

void FileObjectJpegDecChange(int number)
{
    FileJpegSlotView *slot;
    int i = 0;

    while (i < 5) {
        slot = (FileJpegSlotView *)(i * 0xe100 + (int)FileJpegDec);
        i++;
        if (slot->number == number) {
            slot->number = -1;
            FileJpegDecode(number);
            break;
        }
    }
}

#include "tya_draw_gauge.h"
typedef struct FileLoadWorkView {
    unsigned char unmodeled_00[0x20];
    unsigned short slot_status[2];
    int slot;
    unsigned char unmodeled_28[0x38 - 0x28];
    int result;
    int mc_result;
    unsigned char unmodeled_40[0x44 - 0x40];
    signed char hdd;
} FileLoadWorkView;
typedef struct FileLoadRequest {
    int number;
    void *buffer;
    int size;
} FileLoadRequest;
typedef struct FileMcLoadRequest {
    int number;
    int unused;
    void *buffer;
    int size;
} FileMcLoadRequest;
extern TyaGaugeArgs gauge;
extern int xglHddMcUmount(void);
extern int xglHddMcLoadMount(void);
extern int xglHddMcLoadCore(void *save);
extern int xglMcRequest(int port, int command, void *param, int *result);
extern void tyaDrawGauge(TyaGaugeArgs *gauge);
#define SYSOBJ(n) (*(u8 *)((n) + (int)FileObjectDataSystem + 0x67))
#define LOADWORK ((FileLoadWorkView *)FileWork)

int FileObjectAllLoad(void)
{
  u8 *state = FileObjectDataSystem + 2;
  u8 *index = FileObjectDataSystem + 3;
  int new_var;
  unsigned int new_var4;
  FileLoadRequest hdd_request;
  FileMcLoadRequest mc_request;
  int count;
  int i;
  int v;
  unsigned short status;
  volatile TyaGaugeArgs *vg;
  unsigned short *new_var3;
  u8 *new_var2;
  switch (*state)
  {
    case 0:
      vg = &gauge;
      vg->x = 0x7B;
      vg->y = 0x128;
      vg->vertex_attribute = -1;
      vg->width = 0x100;
      vg->height = 8;
      vg->filled_color[0] = 0xA1;
      vg->filled_color[1] = 0xE8;
      vg->filled_color[2] = 0xCD;
      vg->unfilled_color[2] = 0x40;
      vg->unfilled_color[3] = 0x80;
      vg->current = 0;
      vg->maximum = 0;
      vg->filled_color[3] = 0x80;
      vg->unfilled_color[0] = 0x40;
      vg->unfilled_color[1] = 0x40;
      *(int *)((char *)FileWork + 0x38) = 0;
      count = 0;
      i = 0;
      do
    {
      v = *((u8 *) ((i + ((int) FileObjectDataSystem)) + 0x67));
      if (v != 1)
      {
        new_var = v == 0;
        if (new_var)
        {
        }
        else
          if (v < 5)
        {
          count++;
          FileObjectDataInput(i, v);
        }
      }
      else
      {
        gauge.maximum++;
      }
      i++;
    }
    while (i < 99);
      *state = 2;
      *index = 0;
      if (count == 99)
    {
      ((FileLoadWorkView *) FileWork)->result = 0;
      if ((((FileLoadWorkView *) FileWork)->hdd == 0) && ((*((unsigned short *) (v = (((int) ((FileLoadWorkView *) FileWork)) + (((FileLoadWorkView *) FileWork)->slot * 2)) + 0x20))) != 1))
      {
        ((FileLoadWorkView *) FileWork)->result = -1;
      }
      *state = 0;
      return 0;
    }
      if (((FileLoadWorkView *) FileWork)->hdd != 0)
    {
      ((FileLoadWorkView *) FileWork)->result = xglHddMcLoadMount();
    }

    case 2:
      gauge.current++;
      for (; (*index) < 99; (*index)++)
    {
      new_var2 = FileObjectDataSystem;
      if ((*((u8 *) (((*index) + ((int) new_var2)) + 0x67))) == 1)
      {
        *state = 4;
        break;
      }
    }

      if ((*index) == 99)
    {
      *state = 0;
      if (((FileLoadWorkView *) FileWork)->hdd != 0)
      {
        ((FileLoadWorkView *) FileWork)->result = xglHddMcUmount();
      }
      return 0;
    }

    case 4:
      if (((FileLoadWorkView *) FileWork)->hdd != 0)
    {
      hdd_request.number = *index;
      hdd_request.buffer = FileSaveData;
      hdd_request.size = 0x163B0;
      memset(FileSaveData, 0, 0x163B0);
      ((FileLoadWorkView *) FileWork)->result = xglHddMcLoadCore(&hdd_request);
      *state = 6;
    }
      else
    {
      mc_request.number = *index;
      mc_request.buffer = FileSaveData;
      mc_request.size = 0x163B0;
      memset(FileSaveData, 0, 0x163B0);
      xglMcRequest(((FileLoadWorkView *) FileWork)->slot, 4, &mc_request, &((FileLoadWorkView *) FileWork)->mc_result);
      *state = 6;
      break;
    }

    case 6:
      if (((FileLoadWorkView *) FileWork)->hdd != 0)
      {
        if (((FileLoadWorkView *) FileWork)->result != 0)
      {
        new_var4 = (int) FileObjectDataSystem;
        *((u8 *) (((*index) + new_var4) + 0x67)) = 4;
        FileObjectDataInput(*index, 4);
        *state = 9;
        goto checks;
      }
        goto input1;
      }
      else
      {
        new_var3 = ((FileLoadWorkView *) FileWork)->slot_status;
        status = new_var3[((FileLoadWorkView *) FileWork)->slot];
        if (status == 1)
          goto input1;
        if (status != 0xFF)
        {
          *((u8 *) (((*index) + ((int) FileObjectDataSystem)) + 0x67)) = 4;
          FileObjectDataInput(*index, 4);
          ((FileLoadWorkView *) FileWork)->result = -1;
          *state = 9;
        }
        goto checks;
      }
    input1:
      FileObjectDataInput(*index, 1);
      *state = 8;
    checks:
      if ((*state) == 8)
    {
      *state = 2;
      (*index)++;
    }
      if ((*state) == 9)
    {
      *state = 0;
      if (((FileLoadWorkView *) FileWork)->hdd != 0)
      {
        ((FileLoadWorkView *) FileWork)->result = xglHddMcUmount();
      }
      return 0;
    }
      break;

  }

  tyaDrawGauge(&gauge);
  return 1;
}

void FileSinkiSaveDataPush(void)
{
}

typedef struct FileSaveDataView {
    long long version;
    long long checksum;
    int count10;
    int count14;
    unsigned char unmodeled_18[0x60 - 0x18];
    XglClock clock;
} FileSaveDataView;
extern unsigned char SaveData[];
extern unsigned char GameLoopState[];
extern void GamePushSaveDataUser(void);
extern unsigned char *MenuSaveMapNameGet(int map);
extern void xglMcSetMapName(const unsigned char *primary_euc_name, const unsigned char *secondary_euc_name);
extern int MenuScenarioNoGet(void);
extern XglClock *PartyTimeUpDate(void);

void FileSaveDataPush(void)
{
  int i;
  short map;
  unsigned char *name;
  int scenario;
  char rank;
  long long checksum;
  unsigned char *dst;
  unsigned char *src;
  FileSaveDataView *sd;
  GamePushSaveDataUser();
  {
    scenario = (int) SaveData;
    dst = (unsigned char *) scenario + 0x15390;
    src = FileObjectData + 0x68644;
    for (i = 0; i < 0x1000; i++)
    {
      *(dst++) = src[i];
    }
  }
  map = *((short *) (GameLoopState + 0x50));
  sd = (FileSaveDataView *) SaveData;
  *((short *) (((unsigned char *) sd) + 0x10250)) = map;
  name = MenuSaveMapNameGet(map);
  xglMcSetMapName(name, name + 0x21);
  if (map == 10)
  {
    sd->count14++;
  }
  scenario = MenuScenarioNoGet();
  if (scenario >= 0x12D)
    rank = 3;
  else if (scenario >= 0x65)
    rank = 2;
  else
    rank = 1;
  *((long long *) SaveData) = 3;
  SaveData[0x10252] = rank;
  ((FileSaveDataView *) SaveData)->count10++;
  PartyTimeUpDate();
  xglClockRead((XglClock *) (SaveData + 0x60));
  FileCheckSumGet((FileChecksumData *) SaveData, &checksum);
  *((long long *) (SaveData + 8)) = checksum;
}

char *FileSlotNameGet(int slot)
{
    return name_0_0036D628[slot];
}

void TskObjectSet(TskObject *task, TskObjectWorker worker, void *data)
{
    task->data = data;
    task->worker = worker;
    task->state = 0;
}

void tskTskMain(TskObject *task)
{
    TskObjectWorker worker = task->worker;

    if (FileWork->state == 0xff) {
        xglTaskWaitRemove(&task->base);
        return;
    }

    if (task->state != 0) {
        if (task->state != 2)
            return;
    } else {
        worker(task, task->data);
        task->state = 2;
    }

    worker(task, task->data);
}

FileWorkBlock *FileWork = 0;

extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
void eMessageSet(void *message, const char *text);
void eMessageMain(void *message);
void MoveSlide(short *current, short *target, float rate);
extern void endPrintExtFunc(int color, int mode, void *rectangle);
extern char *msg00_1_0036D638[6];
extern int MenuPasLengthGet(const char *text);
struct FilePasWindowUp {
    short x;
    short y;
    int color;
    short width;
    short height;
    char *parameters;
    signed char state;
    unsigned char unmodeled_11[3];
    void *draw;
    void *data;
    unsigned char unmodeled_1c[0x194 - 0x1c];
};
struct FilePassClipUp {
    short x;
    short y;
    int color;
    short width;
    short height;
};
typedef struct FileMessageUp {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[0x02];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x18 - 0x0c];
    char *text;
    unsigned char unmodeled_1c[0x44 - 0x1c];
} FileMessageUp;
typedef struct FilePasWork {
    unsigned char unmodeled_00[0x04];
    short x;
    short y;
    int color;
    struct FilePasWindowUp window;
    FileMessageUp messages[6];
    unsigned char unmodeled_338[0x3c0 - 0x338];
    struct FilePassClipUp rect;
} FilePasWork;

static void MenuFilePas(TskObject *task, void *data)
{
    FilePasWork *work = data;
    int i;
    int lengths[6];
    unsigned short targets[6];
    short slide_target;
    int card_target;

    for (i = 0; i < 6; ++i)
        lengths[i] = MenuPasLengthGet(msg00_1_0036D638[i]);

    switch (task->state) {
    case 0:
        work->x = -272;
        work->y = 8;
        work->color = 0xfffff0;
        WindowDXSet((void *)(&work->window));
        work->window.x = work->x;
        work->window.color = work->color;
        work->window.width = 352;
        work->window.y = work->y;
        work->window.height = 32;
        work->window.state = 1;
        WindowDXMain((void *)(&work->window));
        work->window.state = 3;
        for (i = 0; i < 6; ++i) {
            eMessageSet((void *)(&work->messages[i]), msg00_1_0036D638[i]);
            work->messages[i].mode = 32;
            work->messages[i].x = 288;
            work->messages[i].color = work->color + 2;
            work->messages[i].y = work->y + 3;
        }
        break;
    case 2:
        slide_target = -16;
        for (i = 0; i < 6; ++i)
            targets[i] = 288;
        switch (FileWork->state) {
        case 36:
        case 128:
        case 130:
            targets[FileWork->text_index] = 16;
            break;
        case 37:
        case 38:
        case 39:
        case 40:
        case 41:
        case 42:
        case 44:
        case 45:
        case 46:
        case 80:
        case 82:
        case 136:
        case 144:
        case 145:
        case 146:
        case 147:
        case 148:
        case 149:
        case 150:
        case 152:
        case 160:
        case 161:
        case 176:
        case 184:
            targets[FileWork->text_index] = 16;
            if (FileWork->use_hdd)
                targets[4] = targets[FileWork->text_index] + (unsigned short)lengths[FileWork->text_index];
            else
                targets[2 + FileWork->card_number] = targets[FileWork->text_index] + (unsigned short)lengths[FileWork->text_index];
            break;
        case 132:
        case 134:
        case 135:
            targets[FileWork->text_index] = -(unsigned short)lengths[FileWork->text_index];
            targets[5] = 16;
            card_target = (unsigned short)lengths[5] + 16;
            targets[2 + ((union { int i; char c; } *)&FileWork->card_number)->i] = card_target;
            break;
        default:
            slide_target = -352;
            break;
        }
        MoveSlide(&work->window.x, &slide_target, 3.0f);
        WindowDXMain((void *)(&work->window));
        work->rect.x = work->window.x + 3;
        work->rect.y = work->window.y + 3;
        work->rect.color = work->color;
        work->rect.width = work->window.width - 6;
        work->rect.height = work->window.height - 6;
        endPrintExtFunc(work->color, 101, &work->rect);
        for (i = 0; i < 6; ++i) {
            MoveSlide(&work->messages[i].x, (short *)&targets[i], 3.0f);
            if (work->messages[i].x < 256)
                eMessageMain((void *)(&work->messages[i]));
        }
        endPrintExtFunc(work->color, 102, 0);
        break;
    }
}

extern void MenuInfoWindow(void);
extern void eMessageCpy(char *destination, char *source);
extern void eMessageCat(char *source);
typedef struct UpFileObjectSystem {
    unsigned char unmodeled_00[0x67];
    unsigned char state[99];
} UpFileObjectSystem;
struct FileInfoWindowUp {
    short x;
    short y;
    int color;
    short width;
    short height;
    char *parameters;
    signed char state;
    unsigned char unmodeled_11[3];
    void *draw;
    void *data;
    unsigned char unmodeled_1c[0x194 - 0x1c];
};
struct FileInfoWindowData {
    short x;
    short y;
    unsigned char unmodeled_04[4];
    int mode;
    char *text;
};
struct FileInfoContext {
    unsigned char unmodeled_00[4];
    int work;
    struct FileInfoWindowUp window;
    unsigned char unmodeled_19c[0x1b0 - 0x19c];
    struct FileInfoWindowData contents;
    unsigned char unmodeled_1c0[0x360 - 0x1c0];
    char text[256];
};
struct FileDeviceTexts { char *text[3]; };
struct FileOperationTexts { char *text[3]; };
struct FileSelectionTexts { char *text[4]; };
struct FileListTexts { char *text[2]; };
struct FileConfirmationTexts { char *text[12]; };
struct FileSlotTexts { char *text[3]; };
struct FileSlotStateTexts { char *text[2][3]; };
struct FileCardTexts { char *text[7]; };
struct FileRequestTexts { char *text[9]; };
struct FileFormatTexts { char *text[3]; };
struct FileFinishTexts { char *text[2]; };
struct FileProgressTexts { char *text[4]; };
extern char *D_004C3D48[3];
extern char *D_004C3DF0[3];
extern char *D_004C3EA0[4];
extern char *D_004DAA28[];
extern char *D_004C4018[12];
extern char *D_004C40E0[3];
extern char *D_004C41B0[6];
extern char *D_004C4290[7];
extern char *D_004C42E8[9];
extern char *D_004C4418[3];
extern char *D_004DAA38[];
extern char *D_004C45E0[4];
extern char D_004DAA40[], D_004DAA48[], D_004C4310[];

static void MenuFileInfo(TskObject *task, void *data)
{
    char *device_text[3];
    char *operation_text[3];
    char *selection_text[4];
    char *list_text[2];
    char *confirmation_text[12];
    char *slot_text[3];
    char *slot_state_text[2][3];
    char *card_text[7];
    char *request_text[9];
    char *status_text;
    char *format_text[3];
    char *finish_text[2];
    char *progress_text[4];
    short target_y;
    short target_height;
    struct FileInfoContext *view;

    *(struct FileDeviceTexts *)device_text = *(struct FileDeviceTexts *)D_004C3D48;
    *(struct FileOperationTexts *)operation_text = *(struct FileOperationTexts *)D_004C3DF0;
    *(struct FileSelectionTexts *)selection_text = *(struct FileSelectionTexts *)D_004C3EA0;
    view = data;
    *(struct FileListTexts *)list_text = *(struct FileListTexts *)D_004DAA28;
    *(struct FileConfirmationTexts *)confirmation_text = *(struct FileConfirmationTexts *)D_004C4018;
    *(struct FileSlotTexts *)slot_text = *(struct FileSlotTexts *)D_004C40E0;
    *(struct FileSlotStateTexts *)slot_state_text = *(struct FileSlotStateTexts *)D_004C41B0;
    *(struct FileCardTexts *)card_text = *(struct FileCardTexts *)D_004C4290;
    *(struct FileRequestTexts *)request_text = *(struct FileRequestTexts *)D_004C42E8;
    status_text = D_004C4310;
    *(struct FileFormatTexts *)format_text = *(struct FileFormatTexts *)D_004C4418;
    *(struct FileFinishTexts *)finish_text = *(struct FileFinishTexts *)D_004DAA38;
    *(struct FileProgressTexts *)progress_text = *(struct FileProgressTexts *)D_004C45E0;

    switch (task->state) {
    case 0: {
        struct FileInfoWindowData *contents;

        view->work = 0xfffff0;
        WindowDXSet((void *)(&view->window));
        view->window.x = -16;
        view->window.y = 480;
        view->window.width = 544;
        view->window.height = 54;
        view->window.state = 0;
        contents = &view->contents;
        view->window.draw = MenuInfoWindow;
        view->window.color = view->work;
        view->window.data = contents;
        contents->x = 0;
        view->window.state = 1;
        view->contents.y = 0;
        view->contents.text = 0;
        WindowDXMain((void *)(&view->window));
        view->window.state = 3;
        view->contents.mode = 1;
        memset(view->text, 0, sizeof(view->text));
        break;
    }
    case 2:
        target_y = 378;
        target_height = 54;
        switch (FileWork->state) {
        case 36:
            eMessageCpy(view->text, selection_text[FileWork->text_index]);
            switch (FileWork->device_selection) {
            case 0:
            case 1: {
                unsigned short *card_statuses = FileWork->card_status;

                switch (card_statuses[FileWork->device_selection]) {
                case 0:
                case 129:
                case 130:
                    eMessageCpy(view->text, selection_text[2]);
                    eMessageCat(FileSlotNameGet(FileWork->device_selection));
                    eMessageCat(D_004DAA40);
                    break;
                }
                break;
            }
            case 2:
                eMessageCpy(view->text, device_text[0]);
                break;
            }
            break;
        case 37:
        case 38:
        case 39: {
            unsigned char update_text = FileWork->input_flags & 1;

            if (update_text) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, operation_text[2]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(operation_text[1]);
                } else {
                    eMessageCpy(view->text, operation_text[2]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(operation_text[0]);
                }
            }
            target_y = 352;
            target_height = 78;
            break;
        }
        case 40:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, list_text[FileWork->text_index]);
            target_y = 404;
            target_height = 30;
            break;
        case 41:
            switch (((UpFileObjectSystem *)FileObjectDataSystem)->state[FileWork->selected_slot]) {
            case 2:
                eMessageCpy(view->text, slot_state_text[FileWork->text_index][2]);
                break;
            case 3:
                eMessageCpy(view->text, slot_state_text[FileWork->text_index][0]);
                break;
            case 4:
                eMessageCpy(view->text, slot_state_text[FileWork->text_index][1]);
                break;
            }
            break;
        case 42:
            if (FileWork->input_flags & 1) {
                if (FileWork->text_index == 0) {
                    if (FileWork->use_hdd) {
                        eMessageCpy(view->text, FileSlotNameGet(2));
                        if (((UpFileObjectSystem *)FileObjectDataSystem)->state[FileWork->selected_slot] == 2)
                            eMessageCat(confirmation_text[10]);
                        else
                            eMessageCpy(view->text, confirmation_text[1]);
                    } else {
                        int card_slot = FileWork->selected_slot;

                        if (((UpFileObjectSystem *)FileObjectDataSystem)->state[card_slot] == 2) {
                            eMessageCpy(view->text, confirmation_text[0]);
                            eMessageCat(FileSlotNameGet(FileWork->card_number));
                            eMessageCat(D_004DAA40);
                        } else {
                            eMessageCpy(view->text, confirmation_text[1]);
                        }
                    }
                } else {
                    if (FileWork->use_hdd) {
                        eMessageCpy(view->text, slot_text[2]);
                        eMessageCat(FileSlotNameGet(2));
                        eMessageCat(D_004DAA40);
                    } else {
                        eMessageCpy(view->text, slot_text[0]);
                        eMessageCat(FileSlotNameGet(FileWork->card_number));
                        eMessageCat(D_004DAA40);
                    }
                }
            }
            break;
        case 44:
        case 45:
        case 80:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    if (FileWork->text_index == 0)
                        eMessageCpy(view->text, confirmation_text[4]);
                    else
                        eMessageCpy(view->text, confirmation_text[5]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(confirmation_text[6]);
                } else {
                    if (FileWork->text_index == 0)
                        eMessageCpy(view->text, confirmation_text[7]);
                    else
                        eMessageCpy(view->text, confirmation_text[8]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(confirmation_text[9]);
                }
            }
            target_y = 336;
            target_height = 78;
            break;
        case 132:
            if ((FileWork->input_flags & 1) && !FileWork->use_hdd) {
                eMessageCpy(view->text, confirmation_text[2]);
                eMessageCat(FileSlotNameGet(FileWork->card_number));
                eMessageCat(confirmation_text[11]);
            }
            target_y = 336;
            target_height = 78;
            break;
        case 46:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, confirmation_text[3]);
            break;
        case 82:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, slot_text[1]);
            break;
        case 128:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, card_text[4]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(card_text[6]);
                } else {
                    eMessageCpy(view->text, card_text[0]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(card_text[5]);
                }
            }
            break;
        case 130:
            if (FileWork->input_flags & 1) {
                eMessageCpy(view->text, card_text[1]);
                eMessageCat(FileSlotNameGet(FileWork->card_number));
                eMessageCat(D_004DAA40);
            }
            break;
        case 134:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, card_text[2]);
            break;
        case 135:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, card_text[3]);
            break;
        case 144:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, request_text[FileWork->text_index]);
            break;
        case 145:
            if (FileWork->input_flags & 1) {
                eMessageCpy(view->text, request_text[4]);
                eMessageCat(FileSlotNameGet(FileWork->card_number));
                eMessageCat(D_004DAA48);
            }
            break;
        case 146:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, request_text[6]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(request_text[8]);
                } else {
                    eMessageCpy(view->text, request_text[5]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(request_text[7]);
                }
            }
            break;
        case 148:
        case 149:
        case 150:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, progress_text[FileWork->state - 148]);
            target_y = 336;
            target_height = 78;
            break;
        case 151:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, FileSlotNameGet(2));
                    eMessageCat(request_text[6]);
                } else {
                    char *card_message = view->text;

                    eMessageCpy(card_message, FileSlotNameGet(FileWork->card_number));
                    eMessageCat(request_text[5]);
                }
            }
            break;
        case 160:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, request_text[FileWork->text_index + 2]);
            break;
        case 161:
            if (FileWork->input_flags & 1)
                eMessageCpy(view->text, status_text);
            target_y = 336;
            target_height = 78;
            break;
        case 176:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, FileSlotNameGet(2));
                    eMessageCat(format_text[1]);
                } else {
                    eMessageCpy(view->text, format_text[0]);
                    eMessageCat(FileSlotNameGet(FileWork->card_number));
                    eMessageCat(format_text[2]);
                }
            }
            if (FileWork->use_hdd) {
                target_y = 352;
                target_height = 78;
            } else {
                target_y = 328;
                target_height = 102;
            }
            break;
        case 184:
            if (FileWork->input_flags & 1) {
                if (FileWork->use_hdd) {
                    eMessageCpy(view->text, finish_text[1]);
                    eMessageCat(FileSlotNameGet(2));
                    eMessageCat(D_004DAA48);
                } else {
                    eMessageCpy(view->text, finish_text[0]);
                }
            }
            break;
        default:
            target_y = 480;
            break;
        }
        MoveSlide(&view->window.y, &target_y, 3.0f);
        MoveSlide(&view->window.height, &target_height, 3.0f);
        view->contents.text = view->text;
        WindowDXMain((void *)(&view->window));
        break;
    }
}


struct FilePassWindowU {
    short x;
    short y;
    int color;
    short width;
    short height;
    char *parameters;
    signed char state;
    unsigned char unmodeled_11[3];
    int (*draw)(struct MenuStatusPanelWindow *window,
                struct MenuTopMenuPanel *data);
    void *data;
    unsigned char unmodeled_1c[0x194 - 0x1c];
};

struct FileSelectItem {
    char *text;
    unsigned char unmodeled_04[4];
    signed char color_state;
    unsigned char unmodeled_09[3];
};

struct FileSelectList {
    unsigned char unmodeled_00[2];
    unsigned short layout_mode;
    int selected;
    char *title;
    void *confirmation_data;
    void *items;
    unsigned char unmodeled_14[0x5f8 - 0x14];
};

struct FileSelectContext {
    unsigned char unmodeled_00[2];
    unsigned char initial_flags;
    unsigned char initial_state;
    int work;
    unsigned char animation_flags[4];
    short targets[4];
    struct FilePassWindowU windows[4];
    struct FileSelectList lists[4];
    struct FileSelectItem entries[6];
};

struct FileSelectSource {
    char *message;
    char *title;
};

typedef struct FileWorkBlockFS2 {
    unsigned char input_flags; /* bit 0 enables the explanatory text */
    unsigned char command;
    unsigned char text_index;
    unsigned char state; /* 0xff ends the file menu */
    unsigned char unmodeled_04;
    unsigned char result;
    unsigned char unmodeled_06[3];
    signed char current_slot;
    unsigned char unmodeled_0a[0x13 - 0x0a];
    signed char device_selection;
    unsigned char unmodeled_14;
    signed char action_selection;
    unsigned char unmodeled_16[0x18 - 0x16];
    signed char selected_panel;
    unsigned char unmodeled_19;
    signed char scroll_direction;
    unsigned char unmodeled_1b[2];
    signed char selected_slot;
    unsigned char unmodeled_1e[2];
    unsigned short card_status[2];
    int card_number;
    unsigned char unmodeled_28[0x38 - 0x28];
    int hdd_result;
    int request_result;
    unsigned char unmodeled_40[4];
    signed char use_hdd;
    unsigned char unmodeled_45;
    signed char fast_scroll;
    unsigned char unmodeled_47;
    signed char hide_selection;
    unsigned char unmodeled_49[0x80 - 0x49];
} FileWorkBlockFS2;


extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
extern int MenuSelectWindow();
void MoveSlide(short *current, short *target, float rate);
extern FileWorkBlockFS2 *FileWorkFS2 __asm__("FileWork");

extern char D_004C4858[];
extern char D_004C4870[];
extern char D_004DAA58[];
extern char D_004DAA60[];
extern struct FileSelectSource msg01_2_0036D650[];

/* nested ListColorChage, as upstream (diag run 8) */
static void tskFileSelect(TskObject *task, void *data)
{
    struct FileSelectContext *const view = data;
    int i;

    void ListColorChage(void)
    {
        for (i = 0; i < 2; ++i) {
            unsigned short *card_statuses = FileWorkFS2->card_status;
            int card_status = card_statuses[i];
            switch (card_status) {
            case 0:
            case 129:
            case 130:
                view->entries[i].color_state = 1;
                break;
            default:
                view->entries[i].color_state = 0;
                break;
            }
        }
    }

    switch (task->state) {
    case 0: {
        struct FileSelectSource *confirmation;

        view->work = 0xfffff0;
        view->entries[0].text = D_004C4858;
        view->entries[1].text = D_004C4870;
        for (i = 0; i < 2; ++i)
            view->entries[i].color_state = 0;
        view->entries[i].text = D_004DAA58;
        view->entries[i].color_state = 0;
        view->entries[i + 1].text = 0;
        ListColorChage();
        for (i = 0; i < 2; ++i) {
            WindowDXSet(&view->windows[i]);
            view->windows[i].color = view->work;
            view->windows[i].data = &view->lists[i];
            view->windows[i].draw = MenuSelectWindow;
            view->windows[i].width = 245;
            view->windows[i].height = 78;
            view->lists[i].selected = 0;
            view->lists[i].items = view->entries;
            view->windows[i].parameters = D_004DAA60;
            view->lists[i].layout_mode = 2;
        }
        view->windows[i].height = 102;
        confirmation = msg01_2_0036D650;
        for (i = 2; i < 4; ++i) {
            WindowDXSet(&view->windows[i]);
            view->windows[i].color = view->work;
            view->windows[i].data = &view->lists[i];
            view->windows[i].draw = MenuSelectWindow;
            view->windows[i].width = 207;
            view->windows[i].parameters = D_004DAA60;
            view->lists[i].title = confirmation->title;
            view->windows[i].height = 102;
            view->lists[i].selected = 0;
            view->lists[i].confirmation_data = confirmation->message;
            view->lists[i].layout_mode = 1;
        }
        for (i = 0; i < 4; ++i) {
            view->windows[i].x = -258;
            view->windows[i].y = 0;
            view->windows[i].state = 1;
            WindowDXMain(&view->windows[i]);
            view->windows[i].state = 3;
            view->targets[i] = -258;
            view->animation_flags[i] = 0;
        }
        view->initial_flags = 0;
        view->initial_state = 0;
        return;
    }
    case 2: {
        short targets[4];

        for (i = 0; i < 4; ++i)
            targets[i] = -258;
        switch (FileWorkFS2->state) {
        case 36:
            if (FileWorkFS2->input_flags & 1) {
                view->windows[0].x = 544;
                view->windows[0].y = 176;
            }
            view->lists[0].selected = FileWorkFS2->device_selection;
            ListColorChage();
        case 37:
        case 38:
        case 39:
            if (!FileWorkFS2->hide_selection)
                targets[0] = 128;
            break;
        case 42:
            if (FileWorkFS2->input_flags & 1) {
                view->windows[2].x = 544;
                view->windows[2].y = 224;
            }
            view->lists[2].selected = FileWorkFS2->action_selection;
            targets[2] = 160;
            break;
        case 130:
            if (FileWorkFS2->input_flags & 1) {
                view->windows[2].x = 544;
                view->windows[2].y = 160;
            }
            view->lists[2].selected = FileWorkFS2->action_selection;
            targets[2] = 160;
            break;
        case 160:
            if (FileWorkFS2->input_flags & 1) {
                view->windows[3].x = 544;
                view->windows[3].y = 160;
            }
            view->lists[3].selected = FileWorkFS2->action_selection;
            targets[3] = 160;
            break;
        }
        for (i = 0; i < 4; ++i) {
            MoveSlide(&view->windows[i].x, &targets[i], 3.0f);
            WindowDXMain(&view->windows[i]);
        }
        break;
    }
    }
}

typedef struct FileExWork {
    u8 unmodeled_00[4];
    u32 color;
    u8 active;
    u8 unmodeled_09;
    short target;
    struct { u8 unmodeled_0; u8 flag; u8 unmodeled_2[2]; } message;
    short current;
    short y;
    u32 color2;
    u8 unmodeled_18[0x0c];
    char *text;
} FileExWork;
extern char *msg00_4_0036D658[];
void MoveSlide(short *current, short *target, float rate);
void eMessageSet(void *message, const char *text);
void eMessageMain(void *message);

static void tskFileEx(TskObject *task, FileExWork *work)
{
    switch (task->state) {
    case 0:
        work->color = 0xffffff;
        eMessageSet(&work->message, 0);
        work->color2 = work->color;
        work->message.flag = 0x20;
        work->active = 0;
        return;
    case 2:
    switch (FileWork->state) {
    case 0x2c:
    case 0x2d:
    case 0x50:
    case 0x84:
        if (FileWork->input_flags & 1) {
            if (work->active)
                goto slide;
            work->active = 1;
            work->current = 0x220;
            if (FileWork->state == 0x84) {
                work->target = 0xc0;
                work->text = msg00_4_0036D658[2];
                work->y = 0xc0;
            } else {
                work->target = 0xe0;
                work->y = 0x100;
                work->text = msg00_4_0036D658[FileWork->text_index];
            }
        }
        break;
    default:
        work->target = -0x8c;
        if (work->current == -0x8c)
            work->active = 0;
        break;
    }
    if (work->active) {
slide:
        MoveSlide(&work->current, &work->target, 3.0f);
        eMessageMain(&work->message);
    }
    }
}

struct FilePassWindow {
    short x;
    short y;
    int color;
    short width;
    short height;
    char *parameters;
    signed char state;
    unsigned char unmodeled_11[3];
    int (*draw)(struct MenuStatusPanelWindow *window,
                struct MenuTopMenuPanel *data);
    void *data;
    unsigned char unmodeled_1c[0x194 - 0x1c];
};
struct FileTagFont {
    signed char state;
    signed char sub_state;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    int color;
    signed char channels[4];
    unsigned char unmodeled_10[0x1c - 0x10];
    const char *text;
};
struct FileNumber {
    short x;
    short y;
    int color;
    signed char channels[4];
    signed char mode;
    unsigned char unmodeled_0d;
    unsigned char format;
    unsigned char digits;
    unsigned char flags[4];
    int value;
    unsigned char unmodeled_18[0x90 - 0x18];
};
struct FileFaceSprite {
    signed char state;
    unsigned char unmodeled_01[3];
    short x;
    short y;
    int color;
    signed char channels[4];
    unsigned char unmodeled_10[0x24 - 0x10];
    short sprite_id;
    unsigned char unmodeled_26[2];
};
struct FileRibbon {
    short x;
    short y;
    int color;
    short width;
    short height;
    unsigned char unmodeled_0c[0xe0 - 0x0c];
};
struct FileThumbnailPacket {
    short x;
    short y;
    int color;
    signed char channels[4];
    void *packet;
};
typedef struct FileMessage {
    unsigned char unmodeled_00;
    unsigned char mode;
    unsigned char unmodeled_02[0x02];
    short x;
    short y;
    int color;
    unsigned char unmodeled_0c[0x18 - 0x0c];
    char *text;
    unsigned char unmodeled_1c[0x44 - 0x1c];
} FileMessage;
struct FileThumbnailContext {
    unsigned char state;
    unsigned char visible;
    unsigned char slot;
    unsigned char unmodeled_03;
    short x;
    short y;
    int color;
    struct FilePassWindow window;
    struct FileTagFont labels[2];
    unsigned char unmodeled_1e0[0x200 - 0x1e0];
    FileMessage messages[2];
    struct FileNumber numbers[5];
    unsigned char unmodeled_558[0x638 - 0x558];
    struct FileFaceSprite faces[6];
    unsigned char unmodeled_728[0x778 - 0x728];
    struct FileTagFont slot_label;
    struct FileNumber slot_number;
    struct FileRibbon ribbon;
    struct FileThumbnailPacket thumbnail;
};
typedef struct FileObjectSystem {
    unsigned char unmodeled_00[0x67];
    unsigned char state[99];
} FileObjectSystem;
typedef struct FileWorkBlockTN {
    unsigned char input_flags; /* bit 0 enables the explanatory text */
    unsigned char command;
    unsigned char text_index;
    unsigned char state; /* 0xff ends the file menu */
    unsigned char unmodeled_04;
    unsigned char result;
    unsigned char unmodeled_06[3];
    signed char current_slot;
    unsigned char unmodeled_0a[0x13 - 0x0a];
    signed char device_selection;
    unsigned char unmodeled_14;
    signed char action_selection;
    unsigned char unmodeled_16[0x18 - 0x16];
    signed char selected_panel;
    unsigned char unmodeled_19;
    signed char scroll_direction;
    unsigned char unmodeled_1b[2];
    signed char selected_slot;
    unsigned char unmodeled_1e[2];
    unsigned short card_status[2];
    int card_number;
    unsigned char unmodeled_28[0x38 - 0x28];
    int hdd_result;
    int request_result;
    unsigned char unmodeled_40[4];
    signed char use_hdd;
    unsigned char unmodeled_45;
    signed char fast_scroll;
    unsigned char unmodeled_47;
    signed char hide_selection;
    unsigned char unmodeled_49[0x80 - 0x49];
} FileWorkBlockTN;
typedef struct FileThumbnailSaveEntry {
    FileJpegDecodeSlotView *jpeg;
    unsigned char unmodeled_004[0x1007 - 0x004];
    signed char chapter;
    signed char leader;
    unsigned char unmodeled_1009[3];
    int map_number;
    int party_ids[16];
    unsigned int play_time;
    unsigned char unmodeled_1054[4];
    unsigned int packed_time;
    unsigned char unmodeled_105c[4];
} FileThumbnailSaveEntry;
extern void WindowDXSet(void *window);
extern void WindowDXMain(void *window);
extern void eTagFontSet(void *tag, const char *text);
extern void eTagFontMain(void *tag);
extern void eNumberSet(void *number, int mode);
extern void eNumberMain(void *number);
extern void eSpriteSet(void *sprite, short sprite_id);
extern void eSpriteMain(void *sprite);
extern void eRibbonSet(void *ribbon, int style);
extern void eRibbonMain(void *ribbon);
extern int MenuFaceEpidGet(int sort_entry, int kind);
extern void endPrintExtFunc(int color, int mode, void *rectangle);
extern char D_004C48B8[];
extern char D_004C48C8[];
extern char D_004C48D8[];
extern char D_004C48E8[];
extern char D_004C48F8[];
extern char D_004DAA78[];
extern char *msg00_5_0036D668[];

void FileThumbnailDisp(struct FileThumbnailContext *panel)
{
    int i;
    int party_index;
    int party_count;
    FileThumbnailSaveEntry *entry;
    const unsigned char *map_name;
    unsigned char *window_origin;
    unsigned char *layer_anchor;
    int *slot_layer;
    int *slot_number_layer;
    unsigned char *ribbon_fields;
    unsigned char *leader_fields;
    int ribbon_width;
    int ribbon_height;

    switch (panel->state) {
        do {
    case 0:
        window_origin = (unsigned char *)panel + ((unsigned char *)&panel->window - (unsigned char *)panel);
        WindowDXSet((struct FilePassWindow *)window_origin);
        panel->window.x = 576;
        panel->window.color = panel->color;
        panel->window.width = 336;
        panel->window.parameters = D_004C48B8;
        layer_anchor = (unsigned char *)panel + ((unsigned char *)&panel->window.parameters - (unsigned char *)panel);
        panel->window.height = 96;
        panel->window.state = 1;
        panel->window.y = 0;
        WindowDXMain(&panel->window);
        panel->window.state = 3;
        {
            FileMessage *message = panel->messages;
            for (i = 0; i < 2; ++i) {
                unsigned char *fields;
                int *layer;
                int field_offset;
                eMessageSet(&panel->messages[i], 0);
                field_offset = i * sizeof(FileMessage) +
                    ((unsigned char *)&panel->messages - (unsigned char *)panel) - 16;
                layer = (int *)(layer_anchor + field_offset);
                fields = (unsigned char *)panel + field_offset;
                *layer = panel->color + 2;
                *(unsigned char *)(fields + 16 + ((unsigned char *)&panel->messages[0].mode - (unsigned char *)&panel->messages[0])) = 32;
            }
        }
        {
            struct FileTagFont *label = panel->labels;
            char **text = msg00_5_0036D668;
            unsigned char *layer = layer_anchor +
                ((unsigned char *)&panel->labels[0].color - (unsigned char *)panel) - ((unsigned char *)&panel->window.parameters - (unsigned char *)panel);
            for (i = 0; i < 2; ++i) {
                eTagFontSet((struct ETagFont *)label++, *text++);
                *(int *)layer = panel->color + 2;
                layer += sizeof(struct FileTagFont);
            }
        }
        {
            unsigned char *layer = (unsigned char *)panel +
                ((unsigned char *)&panel->numbers[0].color - (unsigned char *)panel);
            struct FileNumber *number = panel->numbers;
            for (i = 0; i < 5; ++i) {
                eNumberSet(number++, 0);
                *(int *)layer = panel->color + 2;
                layer += sizeof(struct FileNumber);
            }
        }
        slot_layer = &panel->slot_label.color;
        eTagFontSet((struct ETagFont *)&panel->slot_label, D_004DAA78);
        *slot_layer = panel->color + 2;
        slot_number_layer = (int *)(window_origin + ((unsigned char *)&panel->slot_number.color - (unsigned char *)panel) - ((unsigned char *)&panel->window - (unsigned char *)panel));
        ribbon_width = 144;
        ribbon_height = 110;
        eNumberSet(&panel->slot_number, 0);
        *slot_number_layer = panel->color + 2;
        eRibbonSet((struct ERibbonSetData *)&panel->ribbon, 1);
        {
            int field_offset = ((unsigned char *)&panel->ribbon.width - (unsigned char *)panel) - 16;
            int *layer;
            ribbon_fields = (unsigned char *)panel + field_offset;
            layer = (int *)(window_origin + field_offset);
            *(short *)(ribbon_fields + 16) = ribbon_width;
            *layer = panel->color + 1;
            *(short *)(ribbon_fields + 18) = ribbon_height;
        }
        panel->state = 2;
        /* Fall through to the first display. */
    case 2:
        if (!panel->visible)
            break;
        entry = (FileThumbnailSaveEntry *)FileObjectData + panel->slot;
        if (!entry)
            break;
        panel->window.x = panel->x + 144;
        panel->window.y = panel->y;
        WindowDXMain(&panel->window);
        {
            struct FileTagFont *widget = panel->labels;
            int row_offset = 0;
            unsigned char *fields = (unsigned char *)panel +
                ((unsigned char *)&panel->labels - (unsigned char *)panel) - 16;
            for (i = 0; i < 2; ++i) {
                *(short *)(fields + 16 + ((unsigned char *)&panel->labels[0].x - (unsigned char *)&panel->labels[0])) = panel->window.x + 67;
                *(short *)(fields + 16 + ((unsigned char *)&panel->labels[0].y - (unsigned char *)&panel->labels[0])) = panel->window.y + row_offset + 54;
                eTagFontMain((struct ETagFontMainData *)widget++);
                fields += sizeof(struct FileTagFont);
                row_offset += 18;
            }
        }
        switch (((FileObjectSystem *)FileObjectDataSystem)->state[panel->slot]) {
        case 2:
            panel->messages[1].text = 0;
            panel->messages[0].text = D_004C48C8;
            break;
        case 4:
            panel->messages[1].text = 0;
            panel->messages[0].text = D_004C48D8;
            break;
        case 3:
            if (((FileWorkBlockTN *)FileWork)->text_index == 0)
                panel->messages[0].text = D_004C48E8;
            else
                panel->messages[0].text = D_004C48F8;
            panel->messages[1].text = 0;
            break;
        default:
            map_name = MenuSaveMapNameGet(entry->map_number);
            panel->messages[0].text = (char *)map_name;
            panel->messages[1].text = (char *)map_name + 33;
            break;
        }
        {
            FileMessage *widget = panel->messages;
            int row_offset = 0;
            unsigned char *fields = (unsigned char *)panel +
                ((unsigned char *)&panel->messages - (unsigned char *)panel) - 16;
            for (i = 0; i < 2; ++i) {
                *(short *)(fields + 16 + ((unsigned char *)&panel->messages[0].x - (unsigned char *)&panel->messages[0])) = panel->window.x + 3;
                *(short *)(fields + 16 + ((unsigned char *)&panel->messages[0].y - (unsigned char *)&panel->messages[0])) = panel->window.y + row_offset + 3;
                eMessageMain(widget++);
                fields += sizeof(FileMessage);
                row_offset += 24;
            }
        }
        panel->numbers[0].value = entry->packed_time;
        if (((FileObjectSystem *)FileObjectDataSystem)->state[panel->slot] == 1)
            panel->numbers[0].format = 2;
        else
            panel->numbers[0].format = 11;
        panel->numbers[1].value = entry->play_time;
        panel->numbers[1].digits = 9;
        panel->numbers[1].format = 14;
        {
            struct FileNumber *widget = panel->numbers;
            unsigned char *fields = (unsigned char *)panel +
                ((unsigned char *)&panel->numbers - (unsigned char *)panel) - 24;
            int row_offset = 0;
            for (i = 0; i < 2; ++i) {
                *(short *)(fields + 24 + ((unsigned char *)&panel->numbers[0].x - (unsigned char *)&panel->numbers[0])) = panel->window.x + 72;
                *(short *)(fields + 24 + ((unsigned char *)&panel->numbers[0].y - (unsigned char *)&panel->numbers[0])) = panel->window.y + row_offset * 2 + 54;
                eNumberMain((struct ENumberMainData *)widget++);
                fields += sizeof(struct FileNumber);
                row_offset += 9;
            }
        }
        panel->numbers[2].digits = 1;
        panel->numbers[2].value = entry->chapter;
        panel->numbers[2].format = 1;
        panel->numbers[3].digits = 2;
        panel->numbers[3].value = entry->leader;
        panel->numbers[3].format = 1;
        leader_fields = (unsigned char *)panel + ((unsigned char *)&panel->numbers[3].x - (unsigned char *)panel) - 24;
        *(short *)(leader_fields + 24) = panel->window.x + 82;
        *(short *)(leader_fields + 26) = panel->window.y - 13;
        party_index = 0;
        eNumberMain((struct ENumberMainData *)&panel->numbers[3]);
        party_count = 0;
        for (; party_index < 16 && entry->party_ids[party_index] != 0; ++party_index) {
            if (++party_count == 6)
                break;
        }
if (party_count > 0) {
            struct FileFaceSprite *face = panel->faces;
            int column_offset = 0;
            int *member = entry->party_ids;
            unsigned char *layer = (unsigned char *)panel +
                ((unsigned char *)&panel->faces[0].color - (unsigned char *)panel);
            party_index = party_count;
            do {
                eSpriteSet((struct ESprite *)face, (short)MenuFaceEpidGet(*member++, 1));
                face->x = panel->window.x + column_offset * 4 + 196;
                *(int *)layer = panel->window.color + 2;
                face->y = panel->window.y + 58;
                eSpriteMain((struct ESpriteMainData *)face++);
                layer += sizeof(struct FileFaceSprite);
                column_offset += 5;
                --party_index;
            } while (party_index != 0);
        }
        panel->ribbon.x = panel->x;
        panel->ribbon.y = panel->y - 14;
        eRibbonMain((struct ERibbonMainData *)&panel->ribbon);
        panel->slot_label.x = panel->x + 8;
        panel->slot_label.y = panel->y - 14;
        eTagFontMain((struct ETagFontMainData *)&panel->slot_label);
        panel->slot_number.digits = 2;
        panel->slot_number.value = panel->slot + 1;
        panel->slot_number.format = 1;
        panel->slot_number.x = panel->slot_label.x + 30;
        panel->slot_number.y = panel->slot_label.y + 2;
        eNumberMain((struct ENumberMainData *)&panel->slot_number);
        panel->thumbnail.packet = &entry->jpeg->dma_qwc;
        panel->thumbnail.x = panel->x + 16;
        panel->thumbnail.y = panel->y + 1;
        panel->thumbnail.color = panel->color + 2;
        panel->thumbnail.channels[0] = panel->thumbnail.channels[1] =
            panel->thumbnail.channels[2] = panel->thumbnail.channels[3] = -128;
        endPrintExtFunc(panel->thumbnail.color, 13, &panel->thumbnail);
        break;
        } while (0);
    }
}

struct FileCursor {
    signed char state;
    signed char active;
    signed char timer;
    signed char width;
    short x;
    short y;
    int work;
    signed char color[4];
    unsigned char unmodeled_10[0x24 - 0x10];
};

struct FilePassClip {
    short x;
    short y;
    int color;
    short width;
    short height;
};

struct FileDataContext {
    unsigned char state;
    unsigned char visible;
    unsigned char extra_panel;
    unsigned char initial_flags;
    int initial_value;
    int work;
    struct FileThumbnailContext panels[4];
    struct FileCursor cursor;
    struct FilePassClip clip;
};

#define FileWorkTD (*fileWork)
extern void eCursolSet(void *cursor, int width);
extern void eCursolMain(void *cursor);

static void tskFileData(TskObject *task, void *data)
{
    int i;
    int complete;
    short entrance_x;
    short entrance_y;
    short exit_x;
    short exit_y;
    short expanded_y;
    short closing_x;
    struct FileDataContext *view = data;
    FileWorkBlockTN **fileWork = (FileWorkBlockTN **)&FileWork;

    switch (task->state) {
    case 0: {
        int offscreen_x = -512;

        view->work = 0xffff00;
        for (i = 0; i < 4; ++i) {
            view->panels[i].color = view->work;
            view->panels[i].y = 64 + i * 112;
            view->panels[i].x = offscreen_x;
            view->panels[i].slot = 0;
            view->panels[i].state = 0;
            view->panels[i].visible = 0;
        }
        view->panels[3].x = 16;
        eCursolSet((void *)&view->cursor, 0);
        view->cursor.work = view->work + 2;
        view->cursor.state = 32;
        view->visible = 0;
        view->initial_value = 0;
        view->initial_flags = 0;
        view->state = 0;
        view->extra_panel = 0;
        return;
    }
    case 2:
        switch (view->state) {
        case 0:
            view->visible = 0;
            view->state = 1;
        case 1:
            if (FileWorkTD->state == 40) {
                view->visible = 1;
                view->state = 10;
            }
            break;
        case 10:
            for (i = 0; i < 3; ++i) {
                view->panels[i].y = 64 + i * 112;
                view->panels[i].x = 528;
            }
            view->cursor.state = 32;
            view->state = 12;
        case 12: {
            int panel_index;

            complete = 0;
            entrance_x = 16;
            for (panel_index = 0; panel_index < 3; ++panel_index) {
                MoveSlide(&view->panels[panel_index].x, &entrance_x, 3.0f);
                if (view->panels[panel_index].x == entrance_x) {
                    entrance_y = 64 + panel_index * 112;
                    MoveSlide(&view->panels[panel_index].y, &entrance_y, 3.0f);
                    if (view->panels[panel_index].y == entrance_y)
                        ++complete;
                }
            }
            if (complete == 3)
                view->state = 50;
            break;
        }
        case 14: {
            int exit_complete;
            int panel_index;

            exit_complete = 0;
            exit_x = -512;
            exit_y = 64;
            for (panel_index = 0; panel_index < 3; ++panel_index) {
                if (FileWorkTD->selected_panel != panel_index) {
                    MoveSlide(&view->panels[panel_index].x, &exit_x, 3.0f);
                    if (view->panels[panel_index].x == exit_x)
                        ++exit_complete;
                } else {
                    MoveSlide(&view->panels[panel_index].y, &exit_y, 3.0f);
                    if (view->panels[panel_index].y == exit_y)
                        ++exit_complete;
                }
            }
            if (exit_complete == 3)
                view->state = 52;
            break;
        }
        case 16: {

            expanded_y = 112;
            MoveSlide(&view->panels[FileWorkTD->selected_panel].y, &expanded_y, 3.0f);
            if (view->panels[FileWorkTD->selected_panel].y == expanded_y)
                view->state = 52;
            break;
        }
        case 18: {
            int exit_complete;
            int panel_index;

            exit_complete = 0;
            closing_x = -512;
            for (panel_index = 0; panel_index < 3; ++panel_index) {
                MoveSlide(&view->panels[panel_index].x, &closing_x, 3.0f);
                if (view->panels[panel_index].x == closing_x)
                    ++exit_complete;
            }
            if (exit_complete == 3)
                view->state = 0;
            break;
        }
        case 50:
            switch (FileWorkTD->state) {
            case 36:
            case 144:
            case 145:
            case 146:
            case 147:
            case 240:
                view->state = 18;
                break;
            case 41:
            case 42:
                view->state = 14;
                break;
            case 44:
            case 45:
            case 46:
            case 80:
                view->state = 16;
                break;
            }
            if (FileWorkTD->scroll_direction) {
                int scroll_speed;
                int scroll_index;

                for (scroll_index = 0; scroll_index < 3; ++scroll_index) {
                    int direction = FileWorkTD->scroll_direction;

                    view->panels[scroll_index].y += direction * 112;
                }
                {
                    FileWorkBlockTN *scroll_work = FileWorkTD;

                    view->panels[3].x = 16;
                    if (scroll_work->scroll_direction < 0)
                        view->panels[3].y = 288;
                    else
                        view->panels[3].y = 64;
                    scroll_speed = 14;
                    if (scroll_work->fast_scroll)
                        scroll_speed = 28;
                    for (scroll_index = 0; scroll_index < 4; ++scroll_index)
                        view->panels[scroll_index].y += scroll_speed * -FileWorkTD->scroll_direction;
                }
                view->extra_panel = 1;
                view->state = 60;
            }
            break;
        case 52:
            switch (FileWorkTD->state) {
            case 144:
            case 145:
            case 146:
            case 147:
            case 148:
            case 149:
            case 150:
            case 151:
            case 152:
                view->state = 18;
                break;
            case 240:
                view->state = 18;
                break;
            case 44:
            case 45:
            case 46:
                view->state = 16;
                break;
            case 40:
                view->state = 12;
                break;
            }
            break;
        case 60: {
            int scroll_speed = 14;

            if (FileWorkTD->fast_scroll)
                scroll_speed = 28;

            for (i = 0; i < 4; ++i)
                view->panels[i].y += scroll_speed * -FileWorkTD->scroll_direction;
            if (view->panels[0].y == 64) {
                view->extra_panel = 0;
                view->state = 50;
                FileWorkTD->scroll_direction = 0;
            }
            break;
        }
        }
        if (view->visible) {
            FileWorkBlockTN *work;

            for (i = 0; i < 3; ++i)
                view->panels[i].slot = FileObjectDataNoGet(FileWorkTD->current_slot, i);
            if (view->extra_panel) {
                if (FileWorkTD->scroll_direction < 0)
                    view->panels[3].slot = FileObjectDataNoGet(FileWorkTD->current_slot, 3);
                else
                    view->panels[3].slot = FileObjectDataNoGet(FileWorkTD->current_slot, -1);
            }
            view->clip.x = 0;
            view->clip.y = 48;
            view->clip.color = view->work;
            view->clip.width = 512;
            view->clip.height = 336;
            endPrintExtFunc(view->work, 5, &view->clip);
            endPrintExtFunc(view->work, 101, &view->clip);
            for (i = 0; i < view->extra_panel + 3; ++i) {
                view->panels[i].visible = 1;
                FileThumbnailDisp(&view->panels[i]);
            }
            work = FileWorkTD;
            view->cursor.x = view->panels[work->selected_panel].x;
            view->cursor.y = view->panels[work->selected_panel].y + 32;
            eCursolMain((void *)&view->cursor);
            endPrintExtFunc(view->work, 6, 0);
            endPrintExtFunc(view->work, 102, 0);
        }
        break;
    }
}

#define FileWorkBlock MfUpFileWorkBlock
typedef struct FileWorkBlock {
    unsigned char input_flags; /* bit 0 enables the explanatory text */
    unsigned char command;
    unsigned char text_index;
    unsigned char state; /* 0xff ends the file menu */
    unsigned char unmodeled_04;
    unsigned char result;
    unsigned char menu_substate;
    unsigned char dispatch_delay;
    unsigned char unmodeled_08;
    signed char current_slot;
    unsigned char unmodeled_0a[7];
    signed char debug_value_11;
    signed char debug_value_12;
    signed char device_selection;
    signed char debug_value_14;
    signed char action_selection;
    signed char debug_value_16;
    signed char debug_value_17;
    signed char selected_panel;
    signed char panel_count;
    signed char scroll_direction;
    signed char debug_value_1b;
    signed char slot_count;
    signed char selected_slot;
    unsigned char unmodeled_1e[2];
    union FileCardStatus {
        unsigned short status[2];
        unsigned int word;
    } card;
    int card_number;
    unsigned char unmodeled_28[8];
    int available_blocks;
    int directory_result;
    int hdd_result;
    int request_result;
    unsigned short hdd_status;
    unsigned char unmodeled_42[2];
    signed char use_hdd;
    signed char scroll_repeat_count;
    signed char fast_scroll;
    signed char scroll_repeat_direction;
    signed char hide_selection;
    unsigned char unmodeled_49[0x80 - 0x49];
} FileWorkBlock;
#undef FileWorkBlock
#define FileWork ((MfUpFileWorkBlock *)FileWork)
/* FileWork->text_index / MenuFileMain's mode: what the menu was opened for. */
#define FILE_MODE_SAVE 0
#define FILE_MODE_LOAD 1

/* The file menu's state machine (FileWork->state, set from FileWork->command). */
enum FileMenuState {
    FILE_STATE_SELECT_DEVICE = 0x24,      /* memory card 1 / 2 / HDD / Cancel */
    FILE_STATE_CHECK_DEVICE = 0x25,       /* free space and save directory */
    FILE_STATE_LOAD_SLOTS = 0x27,         /* read every slot's header */
    FILE_STATE_SLOT_LIST = 0x28,          /* browse the three-panel slot list */
    FILE_STATE_SLOT_REJECTED = 0x29,      /* the chosen slot cannot be used */
    FILE_STATE_CONFIRM_SLOT = 0x2a,       /* yes/no for the chosen slot */
    FILE_STATE_SAVE_WRITE = 0x2c,
    FILE_STATE_SAVE_CREATE = 0x2d,        /* create the save folder first */
    FILE_STATE_SAVE_DONE = 0x2e,
    FILE_STATE_LOAD_READ = 0x50,
    FILE_STATE_LOAD_DONE = 0x52,
    FILE_STATE_CARD_UNFORMATTED = 0x80,
    FILE_STATE_FORMAT_CONFIRM = 0x82,
    FILE_STATE_FORMAT = 0x84,
    FILE_STATE_FORMAT_DONE = 0x86,
    FILE_STATE_FORMAT_FAILED = 0x87,
    FILE_STATE_ERROR_REQUEST_FAILED = 0x90, /* "Save failed." / "Load failed." */
    FILE_STATE_ERROR_NO_CARD = 0x91,
    FILE_STATE_ERROR_CANNOT_ACCESS = 0x92,
    FILE_STATE_ERROR_HDD_TOO_MANY_SAVES = 0x94,
    FILE_STATE_ERROR_HDD_TOO_MANY_FOLDERS = 0x95,
    FILE_STATE_ERROR_HDD_NO_SPACE = 0x96,
    FILE_STATE_CANCEL_CONFIRM = 0xa0,     /* "Cancel Save." / "Cancel load." */
    FILE_STATE_INSERT_CARD = 0xa1,
    FILE_STATE_NO_CARD_SPACE = 0xb0,      /* not enough free blocks */
    FILE_STATE_NO_SAVE_DATA = 0xb8,
    FILE_STATE_CLOSING = 0xf0,
    FILE_STATE_DONE = 0xff                /* ends the file menu */
};

/* FileObjectDataSystem->state[slot]: what a save slot holds. */
enum FileSlotState {
    FILE_SLOT_EXISTING = 1,  /* a save exists */
    FILE_SLOT_NEW = 2,       /* empty, room for a new save */
    FILE_SLOT_FULL = 3,      /* empty, no room left */
    FILE_SLOT_CORRUPT = 4
};
#define FILE_CARD_SAVE_KB 165
#define FILE_HDD_SAVE_KB 225

/* Thumbnails are 4 KB JPEGs; five decoder buffers hold the slots around
 * the cursor. */
#define FILE_THUMBNAIL_SIZE 0x1000
#define FILE_THUMBNAIL_QUALITY 30

/* MenuFileMain stages its buffers from this fixed main-memory address on: the
 * card data image, the system block, the JPEG decoder slots and the object
 * table, each region starting 0x800-aligned. */
#define FILE_STAGING_BASE 0x01000000
#define FILE_STAGING_ALIGN 0x800
#define FILE_CARD_DATA_BUF_SIZE 0x12000
#define FILE_SYSTEM_BLOCK_SIZE 0x258
#define FILE_JPEG_AREA_SIZE 0x46500
#define FILE_OBJECT_AREA_SIZE 0x696a0
#define FILE_EXIST_CHECK_SIZE 0x2000
#define FILE_JPEG_BUFFERS 5
#define FILE_JPEG_SLOT_SIZE 0xe100

/* Party members stored per save, and the face icons a panel shows. */
#define FILE_PARTY_IDS_MAX 16
#define FILE_PARTY_FACES_MAX 6

/* Slot panels: three visible plus one scrolling in. */
#define FILE_PANELS_VISIBLE 3
#define FILE_PANELS_MAX 4
#define FILE_PANEL_TOP 64
#define FILE_PANEL_PITCH 112
#define FILE_PANEL_X 16
#define FILE_PANEL_OFFSCREEN_X (-512)
#define FILE_PANEL_ENTER_X 528
#define FILE_SCROLL_SPEED 14
#define FILE_SCROLL_SPEED_FAST 28

/* tskFileData's own panel animation states (FileDataContext.state). */
enum FilePanelState {
    FILE_PANEL_HIDDEN = 0,
    FILE_PANEL_WAIT = 1,
    FILE_PANEL_OPEN = 10,
    FILE_PANEL_SLIDE_IN = 12,
    FILE_PANEL_SLIDE_OUT = 14,    /* all but the selected panel leave */
    FILE_PANEL_EXPAND = 16,       /* selected panel moves to the middle */
    FILE_PANEL_CLOSE = 18,
    FILE_PANEL_BROWSE = 50,
    FILE_PANEL_EXPANDED = 52,
    FILE_PANEL_SCROLL = 60
};

/* xglSoundEffectNormalID ids. */
#define FILE_SE_DECIDE 1
#define FILE_SE_CANCEL 2
#define FILE_SE_CURSOR 3
#define FILE_SE_BUZZER 5
#define FILE_SE_SAVE_DONE 0x19
#define FILE_SE_LOAD_DONE 0x1a

/* Slot list: the fast scroll repeats this many steps, and a pad press is
 * accepted again after this many frames (FileWork->menu_substate). */
#define FILE_FAST_SCROLL_STEPS 10

/* Frames FileWork->menu_substate counts down before a state proceeds. */
#define FILE_WAIT_FRAMES 16
#define FILE_INPUT_REPEAT_DELAY 4

/* Yes/No prompts (FileWork->action_selection). */
#define FILE_CHOICE_YES 0
#define FILE_CHOICE_NO 1
#define FILE_CONFIRM_ROWS 2

/* Device list: entries 0 and 1 are the memory card slots, entry 2 is the
 * Cancel row. FileSlotNameGet() names devices instead: 0 and 1 are the card
 * slots, 2 is the hard disk drive. */
#define FILE_DEVICE_CANCEL 2
#define FILE_DEVICE_HDD 2
#define FILE_DEVICE_ROWS 3

/* Frames MenuFile skips before running the state again
 * (FileWork->dispatch_delay): after a state change, and before an HDD
 * request or an error state. */
#define FILE_DELAY_SHORT 12
#define FILE_DELAY_LONG 16

/* FileWork->input_flags bits. */
#define FILE_INPUT_ENTER 1  /* the state was just entered */
#define FILE_INPUT_SKIP 2   /* no pad input on the first frame */

/* GameLoopState.runtime_flags: set while the file menu is up. */
#define FILE_RUNTIME_FLAG_MENU_OPEN 0x04000000

/* FileObjectSystem.load_state: FileObjectAllLoad's progress. */
enum FileLoadState {
    FILE_LOAD_START = 0,
    FILE_LOAD_NEXT_SLOT = 2,
    FILE_LOAD_REQUEST = 4,
    FILE_LOAD_RESULT = 6,
    FILE_LOAD_SLOT_DONE = 8,
    FILE_LOAD_FAILED = 9
};

/* The top banner (MenuFilePas) slides in the names of the current action
 * (Save / Load), the two memory card slots, the HDD and Format. The slot
 * names are FILE_PAS_CARD + card number. */
#define FILE_PAS_MESSAGES 6
#define FILE_PAS_CARD 2
#define FILE_PAS_HDD 4
#define FILE_PAS_FORMAT 5
#define FILE_PAS_X_SHOWN 16
#define FILE_PAS_X_HIDDEN 288

/* Windows and messages slide in from the right edge of the 512-wide screen
 * (x = 544); the device/confirmation lists wait off the left edge. */
#define FILE_SLIDE_IN_X 544
#define FILE_SELECT_HIDDEN_X (-258)

/* Load progress gauge. */
#define FILE_GAUGE_Y 296
#define FILE_GAUGE_WIDTH 256
#define FILE_GAUGE_HEIGHT 8
#define FILE_GAUGE_ALPHA 128
#define FILE_GAUGE_FILLED_RED 161
#define FILE_GAUGE_FILLED_GREEN 232
#define FILE_GAUGE_FILLED_BLUE 205
#define FILE_GAUGE_UNFILLED 64
/* Steps of the card's save-folder creation (create_sub2's subphases). */
#define FILE_CREATE_STEPS 7
#define MC_SAVE_DATA_SIZE 0x163B0
#define MC_ICON_SYS_SIZE 964
#define MC_SAVE_SLOT_COUNT 99
#define MC_LATEST_SLOT_INDEX 100
#define MC_TYPE_PS1 1
#define MC_TYPE_PS2 2
#define MC_TYPE_POCKETSTATION 3
#define MC_STATUS_NONE 0
#define MC_STATUS_READY 1
#define MC_STATUS_UNFORMATTED 2
#define MC_STATUS_ERROR 0x80
#define MC_STATUS_PS1_CARD 0x81
#define MC_STATUS_POCKETSTATION 0x82
#define MC_STATUS_BUSY 0xff
#define MC_STATUS_BUSY_BOTH 0xFF00FF
#define MC_REQUEST_FREE_SPACE 1
#define MC_REQUEST_DIRECTORY 2
#define MC_REQUEST_PATH_EXISTS 3
#define MC_REQUEST_LOAD 4
#define MC_REQUEST_LOAD_PATH 5
#define MC_REQUEST_SAVE 6
#define MC_REQUEST_SAVE_PATH 7
#define MC_REQUEST_CREATE 8
#define MC_REQUEST_FORMAT 9
#define MC_REQUEST_UNFORMAT 10
#define HDD_CHECK_ACCESS (-1)
#define HDD_CHECK_TOO_MANY_FOLDERS (-2)
#define HDD_CHECK_TOO_MANY_SAVES (-3)
#define HDD_CHECK_NO_SPACE (-4)
#define HDD_CREATE_ACCESS (-14)
#define HDD_CREATE_TOO_MANY_FOLDERS (-9)
#define HDD_CREATE_TOO_MANY_SAVES (-13)
#define HDD_CREATE_NO_SPACE (-15)
#define FILE_SLOT_COUNT 99
typedef struct McRequestArguments { int slot; const unsigned char *path; int buffer; int size; } McRequestArguments;
struct MfHddCheckState { int status; union { unsigned char *bytes; void *info; } transfer; int dataSize; };
struct HddExistRequest { int card; unsigned char *found; };
struct HddSaveRequest { int card; const void *data; int size; };
struct HddLoadRequest { int card; void *data; int size; };
#define HddCheckState MfHddCheckState
typedef struct { u8 pad_00[0x28]; u16 half_28; u16 half_2a; unsigned char unmodeled_2c[4]; u16 half_30; u16 mappedPressed; u16 mappedRepeat; } MfPadPrefix;
extern MfPadPrefix PadData;
#define PAD_CIRCLE   0x0020
#define PAD_CROSS    0x0040
#define PAD_UP       0x1000
#define PAD_DOWN     0x4000
#define FILE_UNIT_MAP_NO 10
extern u8 *FileExistCheckWork;
typedef struct MfFileObjectSystem { unsigned char unmodeled_00[2]; unsigned char load_state; unsigned char load_index; signed char card_status[99]; unsigned char state[99]; } MfFileObjectSystem;
#define FileObjectDataSystem ((MfFileObjectSystem *)FileObjectDataSystem)
#define FileObjectData ((FileObjectEntryView *)FileObjectData)
#define FileSaveRecord FileChecksumData
extern void *FileCardDataBuf;
extern const char D_004DAA80[];
extern const char D_004DAA88[];
extern const char D_004DAA90[];
#define shortNumberFormat D_004DAA80
#define memoryCardMarkerText D_004DAA88
#define wideNumberFormat D_004DAA90
typedef union FileCardNumber {
    int number;
} FileCardNumber;
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern void xglFontDebugPrintf(int x, int y, const char *format, ...);
extern void xglFontDebugHex(int x, int y, int value, int digits);
void MenuFile(void) {
    unsigned char delay;
    unsigned char state;
    int card_status;
    union FileCardStatus *card;
    McRequestArguments card_request;
    union {
        struct HddExistRequest exist;
        struct HddCheckState create;
        struct HddLoadRequest load;
        struct HddSaveRequest save;
    } hdd_request;

    if (FileWork->state != FileWork->command) {
        FileWork->state = FileWork->command;
        FileWork->input_flags |= FILE_INPUT_ENTER;
    } else {
        FileWork->input_flags &= 0xfe;
    }
    FileWork->card.word = xglMcMain();
    delay = FileWork->dispatch_delay;
    if (delay != 0) {
        FileWork->dispatch_delay = delay - 1;
        return;
    }
    state = FileWork->state;
    switch (state) {
    case FILE_STATE_SELECT_DEVICE: {
        signed char selection;
        unsigned char input_flags = FileWork->input_flags;
        if (input_flags & FILE_INPUT_ENTER) {
            if (FileWork->use_hdd) {
                FileWork->device_selection = FILE_DEVICE_CANCEL;
            } else {
                FileWork->device_selection = FileWork->card_number;
            }
            FileWork->hdd_status = (unsigned short) xglHddCheck();
            FileWork->dispatch_delay = FILE_DELAY_SHORT;
            FileWork->hide_selection = 0;
            break;
        }
        else if (input_flags & FILE_INPUT_SKIP) {
            break;
        } else {
            FileWork->device_selection = MenuSelectMove(FileWork->device_selection, FILE_DEVICE_ROWS, 0);
            if (PadData.half_2a & PAD_CIRCLE) {
                selection = FileWork->device_selection;
                switch (selection) {
                    case 0:
                    case 1:
                        FileWork->use_hdd = 0;
                        FileWork->card_number = FileWork->device_selection;
                        card = &FileWork->card;
                        card_status = card->status[FileWork->card_number];
                        switch (card_status) {
                        case MC_STATUS_NONE:
                        case MC_STATUS_PS1_CARD:
                        case MC_STATUS_POCKETSTATION:
                            xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                            break;
                        case MC_STATUS_UNFORMATTED:
                            if (FileWork->text_index == FILE_MODE_SAVE) {
                                FileWork->command = FILE_STATE_CARD_UNFORMATTED;
                            } else {
                                FileWork->command = FILE_STATE_NO_SAVE_DATA;
                            }
                            xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                            break;
                        case MC_STATUS_BUSY:
                            break;
                        default:
                            FileWork->command = FILE_STATE_CHECK_DEVICE;
                            xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                            break;
                        }
                        break;
                    case FILE_DEVICE_CANCEL:
                        xglSoundEffectNormalID(FILE_SE_CANCEL, 0);
                        FileWork->command = FILE_STATE_CLOSING;
                        if (FileWork->text_index == FILE_MODE_LOAD) {
                            FileWork->result = 1;
                        }
                        break;
                    default:
                        break;
                }
            }
            if (PadData.half_2a & PAD_CROSS) {
                FileWork->command = FILE_STATE_CLOSING;
                xglSoundEffectNormalID(FILE_SE_CANCEL, 0);
                if (FileWork->text_index == FILE_MODE_LOAD) {
                    FileWork->result = 1;
                }
            }
        }
        break;
    }
    case FILE_STATE_CHECK_DEVICE:
    {
        int hdd_slot;
        int card_slot;
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            if (FileWork->use_hdd) {
                FileWork->dispatch_delay = FILE_DELAY_SHORT;
                FileWork->available_blocks = xglHddMcGetFree();
            } else {
                xglMcRequest(FileWork->card_number, MC_REQUEST_FREE_SPACE, 0,
                             &FileWork->available_blocks);
                card_request.slot = -1;
                card_request.buffer = (int)FileExistCheckWork;
                xglMcRequest(FileWork->card_number, MC_REQUEST_DIRECTORY, &card_request,
                             &FileWork->directory_result);
                FileWork->menu_substate = 0;
            }
        } else if (FileWork->use_hdd) {
            if (FileWork->available_blocks < 0) {
                if (FileWork->text_index == FILE_MODE_SAVE) {
                    switch (FileWork->available_blocks) {
                    case HDD_CHECK_ACCESS:
                        FileWork->command = FILE_STATE_ERROR_CANNOT_ACCESS;
                        break;
                    case HDD_CHECK_TOO_MANY_FOLDERS:
                        FileWork->command = FILE_STATE_ERROR_HDD_TOO_MANY_FOLDERS;
                        break;
                    case HDD_CHECK_TOO_MANY_SAVES:
                        FileWork->command = FILE_STATE_ERROR_HDD_TOO_MANY_SAVES;
                        break;
                    case HDD_CHECK_NO_SPACE:
                        FileWork->command = FILE_STATE_ERROR_HDD_NO_SPACE;
                        break;
                    }
                } else {
                    switch (FileWork->available_blocks) {
                    case HDD_CHECK_ACCESS:
                        FileWork->command = FILE_STATE_ERROR_CANNOT_ACCESS;
                        break;
                    case HDD_CHECK_TOO_MANY_FOLDERS:
                    case HDD_CHECK_TOO_MANY_SAVES:
                    case HDD_CHECK_NO_SPACE:
                        FileWork->command = FILE_STATE_NO_SAVE_DATA;
                        break;
                    }
                }
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                FileWork->dispatch_delay = FILE_DELAY_LONG;
                break;
            }
            hdd_request.exist.card = -1;
            hdd_request.exist.found = FileExistCheckWork;
            FileWork->directory_result = xglHddMcExist(&hdd_request.exist);
            if (FileWork->directory_result == 0) {
                if (FileWork->text_index == FILE_MODE_SAVE) {
                if (FileWork->available_blocks < FILE_HDD_SAVE_KB) {
                    FileWork->command = FILE_STATE_NO_CARD_SPACE;
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                } else {
                    for (hdd_slot = 0; hdd_slot < FILE_SLOT_COUNT; hdd_slot++) {
                        if (FileWork->available_blocks >= FILE_HDD_SAVE_KB) {
                            FileObjectDataSystem->state[hdd_slot] = FILE_SLOT_NEW;
                            FileWork->available_blocks -= FILE_HDD_SAVE_KB;
                        } else {
                            FileObjectDataSystem->state[hdd_slot] = FILE_SLOT_FULL;
                        }
                    }
                    FileExistCheckWork[MC_LATEST_SLOT_INDEX] = 0;
                    FileWork->command = FILE_STATE_LOAD_SLOTS;
                }
                } else {
                    FileWork->command = FILE_STATE_NO_SAVE_DATA;
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                }
            } else if (FileWork->directory_result < 0) {
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                FileWork->command = FILE_STATE_ERROR_CANNOT_ACCESS;
            } else {
                FileWork->selected_slot = 0;
                FileWork->slot_count = FILE_SLOT_COUNT;
                if (FileWork->directory_result == 0 &&
                    FileWork->available_blocks < FILE_HDD_SAVE_KB) {
                    if (FileWork->text_index == FILE_MODE_SAVE) {
                        FileWork->command = FILE_STATE_NO_CARD_SPACE;
                    } else {
                        FileWork->command = FILE_STATE_NO_SAVE_DATA;
                    }
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                    break;
                }
                for (hdd_slot = 0; hdd_slot < FILE_SLOT_COUNT; hdd_slot++) {
                    if (FileExistCheckWork[hdd_slot] != 0) {
                        FileObjectDataSystem->state[hdd_slot] = FILE_SLOT_EXISTING;
                    } else if (FileWork->available_blocks >= FILE_HDD_SAVE_KB) {
                        FileObjectDataSystem->state[hdd_slot] = FILE_SLOT_NEW;
                        FileWork->available_blocks -= FILE_HDD_SAVE_KB;
                    } else {
                        FileObjectDataSystem->state[hdd_slot] = FILE_SLOT_FULL;
                    }
                }
                if (FileWork->text_index == FILE_MODE_SAVE) {
                    FileWork->command = FILE_STATE_LOAD_SLOTS;
                } else {
                    switch (FileWork->directory_result) {
                    default:
                        FileWork->command = FILE_STATE_LOAD_SLOTS;
                        break;
                    case 0:
                        FileWork->command = FILE_STATE_NO_SAVE_DATA;
                        xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                        break;
                    }
                }
            }
        } else {
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            switch (card_status) {
            case MC_STATUS_READY:
                if (FileWork->menu_substate != 0) {
                    FileWork->selected_slot = 0;
                    FileWork->slot_count = FILE_SLOT_COUNT;
                    if (FileWork->directory_result == 0 &&
                        FileWork->available_blocks < FILE_CARD_SAVE_KB) {
                        if (FileWork->text_index == FILE_MODE_SAVE) {
                            FileWork->command = FILE_STATE_NO_CARD_SPACE;
                        } else {
                            FileWork->command = FILE_STATE_NO_SAVE_DATA;
                        }
                        xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                        break;
                    } else {
                        for (card_slot = 0; card_slot < FILE_SLOT_COUNT; card_slot++) {
                            if (FileExistCheckWork[card_slot] != 0) {
                                FileObjectDataSystem->state[card_slot] = FILE_SLOT_EXISTING;
                            } else if (FileWork->available_blocks >= FILE_CARD_SAVE_KB) {
                                FileObjectDataSystem->state[card_slot] = FILE_SLOT_NEW;
                                FileWork->available_blocks -= FILE_CARD_SAVE_KB;
                            } else {
                                FileObjectDataSystem->state[card_slot] = FILE_SLOT_FULL;
                            }
                        }
                        if (FileWork->directory_result < 0) {
                            FileWork->command = FILE_STATE_ERROR_NO_CARD;
                            xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                        } else {
                            if (FileWork->text_index == FILE_MODE_SAVE) {
                                FileWork->command = FILE_STATE_LOAD_SLOTS;
                            } else {
                                switch (FileWork->directory_result) {
                                default:
                                    FileWork->command = FILE_STATE_LOAD_SLOTS;
                                    break;
                                case 0:
                                    FileWork->command = FILE_STATE_NO_SAVE_DATA;
                                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                                    break;
                                }
                            }
                        }
                    }
                }
                FileWork->menu_substate = 1;
                break;
            case MC_STATUS_NONE:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            case MC_STATUS_BUSY:
                break;
            default:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            }
        }
        break;
    }
    case FILE_STATE_DONE:
        break;
    case FILE_STATE_LOAD_SLOTS:
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            FileWork->selected_slot = FileExistCheckWork[MC_LATEST_SLOT_INDEX];
            FileWork->panel_count = FILE_PANELS_VISIBLE;
            FileWork->selected_panel = 0;
            FileWork->scroll_direction = 0;
            FileWork->current_slot = FileWork->selected_slot;
            FileObjectDataClear();
            FileObjectDataSystem->unmodeled_00[1] = FileWork->current_slot;
        } else if (FileWork->use_hdd) {
            int load_result = FileObjectAllLoad();
            if (load_result == 0) {
                if (FileWork->hdd_result >= 0) {
                    FileWork->slot_count = FILE_SLOT_COUNT;
                    FileObjectJpegSet();
                    FileWork->command = FILE_STATE_SLOT_LIST;
                    FileWork->dispatch_delay = FILE_DELAY_SHORT;
                }
            }
            if (FileWork->hdd_result != 0) {
                FileWork->command = FILE_STATE_ERROR_CANNOT_ACCESS;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
            }
        } else {
            unsigned short loaded_card_status;
            int load_result = FileObjectAllLoad();
            if (load_result == 0) {
                if (FileWork->hdd_result >= 0) {
                    FileWork->slot_count = FILE_SLOT_COUNT;
                    FileObjectJpegSet();
                    FileWork->command = FILE_STATE_SLOT_LIST;
                    FileWork->dispatch_delay = FILE_DELAY_SHORT;
                }
            }
            if (FileWork->hdd_result >= 0) {
                break;
            }
            card = &FileWork->card;
            loaded_card_status = card->status[FileWork->card_number];
            switch (loaded_card_status) {
            default:
                FileWork->command = FILE_STATE_ERROR_CANNOT_ACCESS;
                FileWork->slot_count = 0;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            case MC_STATUS_BUSY:
                break;
            case MC_STATUS_NONE:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            }
        }
        break;
    case FILE_STATE_SLOT_LIST:
    {
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            FileWork->scroll_direction = 0;
        }
        if ((FileWork->input_flags & FILE_INPUT_SKIP) == 0) {
            if (FileWork->scroll_direction == 0) {
                unsigned short action = 0;

                if (FileWork->fast_scroll != 0) {
                    if (FileWork->scroll_repeat_count == FILE_FAST_SCROLL_STEPS) {
                        FileWork->fast_scroll = 0;
                    } else {
                        FileWork->scroll_repeat_count++;
                        action = FileWork->scroll_repeat_direction < 0
                                     ? PAD_UP
                                     : PAD_DOWN;
                    }
                    FileWork->menu_substate = 0;
                } else {
                    if (FileWork->menu_substate == 0) {
                        u16 menu_input = PadData.half_30;

                        if (menu_input & PAD_UP) {
                            if (FileWork->selected_panel == 0 ||
                                (PadData.mappedRepeat & PAD_UP)) {
                                action = PAD_UP;
                            }
                        } else if (menu_input & PAD_DOWN) {
                            if (FileWork->selected_panel == FILE_PANELS_VISIBLE - 1 ||
                                (PadData.mappedRepeat & PAD_DOWN)) {
                                action = PAD_DOWN;
                            }
                        } else {
                            action = PAD_CROSS;
                            if ((menu_input & PAD_CROSS) == 0) {
                                action = menu_input & PAD_CIRCLE;
                            }
                        }
                        if (action != 0) {
                            FileWork->menu_substate = FILE_INPUT_REPEAT_DELAY;
                        }
                    } else {
                        FileWork->menu_substate--;
                    }
                }
                if (action == PAD_UP) {
                    FileWork->selected_slot = FileObjectDataNoGet(
                        FileWork->selected_slot, -1);
                    if (FileWork->selected_panel == 0) {
                        FileWork->scroll_direction = -1;
                        FileWork->current_slot = FileObjectDataNoGet(
                            FileWork->current_slot, -1);
                        FileObjectJpegSet();
                    } else {
                        FileWork->selected_panel--;
                    }
                    xglSoundEffectNormalID(FILE_SE_CURSOR, 0);
                }
                if (action == PAD_DOWN) {
                    FileWork->selected_slot = FileObjectDataNoGet(
                        FileWork->selected_slot, 1);
                    if (FileWork->selected_panel ==
                        FileWork->panel_count - 1) {
                        FileWork->scroll_direction = 1;
                        FileWork->current_slot = FileObjectDataNoGet(
                            FileWork->current_slot, 1);
                        FileObjectJpegSet();
                    } else {
                        FileWork->selected_panel++;
                    }
                    xglSoundEffectNormalID(FILE_SE_CURSOR, 0);
                }
                if (action == PAD_CROSS) {
                    FileWork->command = FILE_STATE_SELECT_DEVICE;
                    xglSoundEffectNormalID(FILE_SE_CANCEL, 0);
                }
                if (action == PAD_CIRCLE) {
                    unsigned char object_status =
                        FileObjectDataSystem->state[FileWork->selected_slot];
                    switch (object_status) {
                    case FILE_SLOT_NEW:
                        if (FileWork->text_index == FILE_MODE_SAVE) {
                            FileWork->command = FILE_STATE_CONFIRM_SLOT;
                            xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                        } else {
                            FileWork->command = FILE_STATE_SLOT_REJECTED;
                            xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                        }
                        break;
                    case FILE_SLOT_FULL:
                    case FILE_SLOT_CORRUPT:
                        FileWork->command = FILE_STATE_SLOT_REJECTED;
                        xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                        break;
                    default:
                        if (FileWork->text_index == FILE_MODE_SAVE) {
                            FileWork->command = FILE_STATE_CONFIRM_SLOT;
                            xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                        } else {
                            signed char selected_slot = FileWork->selected_slot;
                            FileObjectEntryView *selected_entry =
                                &FileObjectData[selected_slot];
                            if (selected_entry->map_number == FILE_UNIT_MAP_NO) {
                                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                            } else {
                                FileWork->command = FILE_STATE_CONFIRM_SLOT;
                                xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                            }
                        }
                        break;
                    }
                }
            } else {
                FileWork->menu_substate = 0;
            }
        }
        if (FileWork->use_hdd == 0) {
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            switch (card_status) {
            case MC_STATUS_READY:
            case MC_STATUS_BUSY:
                break;
            case MC_STATUS_NONE:
            default:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            }
        }
        break;
    }
    case FILE_STATE_SLOT_REJECTED: {
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            FileWork->dispatch_delay = FILE_DELAY_SHORT;
        } else if (FileWork->use_hdd) {
            if (PadData.half_2a & PAD_CIRCLE) {
                FileWork->command = FILE_STATE_SLOT_LIST;
                FileWork->dispatch_delay = FILE_DELAY_SHORT;
                xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
            }
        } else {
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            switch (card_status) {
            case MC_STATUS_READY:
                if (PadData.half_2a & PAD_CIRCLE) {
                    FileWork->command = FILE_STATE_SLOT_LIST;
                    FileWork->dispatch_delay = FILE_DELAY_SHORT;
                    xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                }
                break;
            case MC_STATUS_BUSY:
                break;
            case MC_STATUS_NONE:
            default:
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                break;
            }
        }
        break;
    }
    case FILE_STATE_CONFIRM_SLOT: {
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            FileWork->dispatch_delay = 10;
            if (FileWork->text_index == FILE_MODE_SAVE) {
                FileWork->action_selection = FILE_CHOICE_NO;
            } else {
                FileWork->action_selection = FILE_CHOICE_YES;
            }
            if (!FileWork->use_hdd) {
                xglMcRequest(FileWork->card_number, MC_REQUEST_FREE_SPACE, 0,
                             &FileWork->available_blocks);
            }
        } else if (FileWork->use_hdd) {
            if (!(FileWork->input_flags & FILE_INPUT_SKIP)) {
                FileWork->action_selection =
                    MenuSelectMove(FileWork->action_selection, FILE_CONFIRM_ROWS, 0);
                if (PadData.half_2a & PAD_CIRCLE) {
                    if (FileWork->action_selection == FILE_CHOICE_YES) {
                        if (FileWork->text_index != FILE_MODE_SAVE) {
                            FileWork->command = FILE_STATE_LOAD_READ;
                        } else {
                            switch (FileObjectDataSystem->state[FileWork->selected_slot]) {
                            case FILE_SLOT_NEW:
                                FileWork->command = FILE_STATE_SAVE_CREATE;
                                break;
                            default:
                                FileWork->command = FILE_STATE_SAVE_CREATE;
                                break;
                            }
                        }
                        xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                    } else {
                        FileWork->command = FILE_STATE_SLOT_LIST;
                        FileWork->dispatch_delay = FILE_DELAY_SHORT;
                        xglSoundEffectNormalID(FILE_SE_CANCEL, 0);
                    }
                }
                if (PadData.half_2a & PAD_CROSS) {
                    FileWork->command = FILE_STATE_SLOT_LIST;
                    FileWork->dispatch_delay = FILE_DELAY_SHORT;
                    xglSoundEffectNormalID(FILE_SE_CANCEL, 0);
                }
            }
        } else {
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            switch (card_status) {
            case MC_STATUS_NONE:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            case MC_STATUS_READY:
            if (!(FileWork->input_flags & FILE_INPUT_SKIP)) {
                FileWork->action_selection =
                    MenuSelectMove(FileWork->action_selection, FILE_CONFIRM_ROWS, 0);
                if (PadData.half_2a & PAD_CIRCLE) {
                        if (FileWork->action_selection == FILE_CHOICE_YES) {
                            if (FileWork->text_index != FILE_MODE_SAVE) {
                                FileWork->command = FILE_STATE_LOAD_READ;
                            } else if (FileObjectDataSystem->state[FileWork->selected_slot] != FILE_SLOT_NEW ||
                                       FileWork->available_blocks >= FILE_CARD_SAVE_KB) {
                                FileWork->command = FILE_STATE_SAVE_CREATE;
                            } else {
                                FileWork->command = FILE_STATE_NO_CARD_SPACE;
                            }
                        xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                    } else {
                        FileWork->command = FILE_STATE_SLOT_LIST;
                        FileWork->dispatch_delay = FILE_DELAY_SHORT;
                        xglSoundEffectNormalID(FILE_SE_CANCEL, 0);
                    }
                }
                if (PadData.half_2a & PAD_CROSS) {
                    FileWork->dispatch_delay = FILE_DELAY_SHORT;
                    FileWork->command = FILE_STATE_SLOT_LIST;
                    xglSoundEffectNormalID(FILE_SE_CANCEL, 0);
                }
            }
                break;
            case MC_STATUS_BUSY:
                break;
            default:
                FileWork->command = FILE_STATE_ERROR_REQUEST_FAILED;
                break;
            }
        }
        break;
    }
    case FILE_STATE_SAVE_WRITE:
    {
        int hdd_existing_slot;
        int card_existing_slot;
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            if (FileWork->use_hdd) {
                FileWork->dispatch_delay = FILE_DELAY_LONG;
            } else {
                card_request.slot = FileWork->selected_slot;
                card_request.buffer = (int)SaveData;
                card_request.size = MC_SAVE_DATA_SIZE;
                xglMcRequest(FileWork->card_number, MC_REQUEST_SAVE, &card_request,
                             &FileWork->hdd_result);
                xglMcRequest(FileWork->card_number, MC_REQUEST_FREE_SPACE, 0,
                             &FileWork->available_blocks);
                FileWork->menu_substate = 0;
                tyaDrawGauge(&gauge);
            }
        } else if (FileWork->use_hdd) {
            hdd_request.save.card = FileWork->selected_slot;
            hdd_request.save.data = SaveData;
            hdd_request.save.size = MC_SAVE_DATA_SIZE;
            FileWork->directory_result = xglHddMcSave(&hdd_request.save);
            if (FileWork->directory_result != 0) {
                FileWork->command = FILE_STATE_ERROR_REQUEST_FAILED;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
            } else {
                xglSoundEffectNormalID(FILE_SE_SAVE_DONE, 0);
                FileWork->available_blocks = xglHddMcGetFree();
                *FileSaveData = *(FileSaveRecord *) SaveData;
                hdd_existing_slot = 0;
                FileObjectDataInput(FileWork->selected_slot, FILE_SLOT_EXISTING);
                FileObjectDataSystem->state[FileWork->selected_slot] = FILE_SLOT_EXISTING;
                FileObjectJpegDecChange(FileWork->selected_slot);
                    for (; hdd_existing_slot < FILE_SLOT_COUNT; hdd_existing_slot++) {
                        if (FileObjectDataSystem->state[hdd_existing_slot] == FILE_SLOT_NEW) {
                        FileWork->available_blocks -= FILE_HDD_SAVE_KB;
                    }
                }
                for (hdd_existing_slot = 0; hdd_existing_slot < FILE_SLOT_COUNT; hdd_existing_slot++) {
                    if (FileObjectDataSystem->state[hdd_existing_slot] == FILE_SLOT_FULL &&
                        FileWork->available_blocks >= FILE_HDD_SAVE_KB) {
                        FileObjectDataInput(hdd_existing_slot, FILE_SLOT_NEW);
                        FileObjectDataSystem->state[hdd_existing_slot] = FILE_SLOT_NEW;
                        FileObjectJpegDecChange(hdd_existing_slot);
                        FileWork->available_blocks -= FILE_HDD_SAVE_KB;
                    }
                }
                FileWork->command = FILE_STATE_SAVE_DONE;
            }
        } else {
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            if (card_status != MC_STATUS_READY) {
                if (card_status != MC_STATUS_BUSY) {
                    FileWork->command = FILE_STATE_ERROR_REQUEST_FAILED;
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                } else {
                    tyaDrawGauge(&gauge);
                }
            } else if (FileWork->menu_substate == 0) {
                xglSoundEffectNormalID(FILE_SE_SAVE_DONE, 0);
                *FileSaveData = *(FileSaveRecord *) SaveData;
                FileObjectDataInput(FileWork->selected_slot, FILE_SLOT_EXISTING);
                FileObjectDataSystem->state[FileWork->selected_slot] = FILE_SLOT_EXISTING;
                FileObjectJpegDecChange(FileWork->selected_slot);
                FileWork->menu_substate = 1;
                tyaDrawGauge(&gauge);
            } else {
                for (card_existing_slot = 0; card_existing_slot < FILE_SLOT_COUNT; card_existing_slot++) {
                    if (FileObjectDataSystem->state[card_existing_slot] == FILE_SLOT_NEW) {
                        FileWork->available_blocks -= FILE_CARD_SAVE_KB;
                    }
                }
                for (card_existing_slot = 0; card_existing_slot < FILE_SLOT_COUNT; card_existing_slot++) {
                    if (FileObjectDataSystem->state[card_existing_slot] == FILE_SLOT_FULL &&
                        FileWork->available_blocks >= FILE_CARD_SAVE_KB) {
                        FileObjectDataInput(card_existing_slot, FILE_SLOT_NEW);
                        FileObjectDataSystem->state[card_existing_slot] = FILE_SLOT_NEW;
                        FileObjectJpegDecChange(card_existing_slot);
                        FileWork->available_blocks -= FILE_CARD_SAVE_KB;
                    }
                }
                FileWork->command = FILE_STATE_SAVE_DONE;
            }
        }
        break;
    }
    case FILE_STATE_SAVE_CREATE: {
        int create_result;

        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            if (FileWork->use_hdd) {
                FileWork->menu_substate = FILE_WAIT_FRAMES;
            } else {
                card_request.slot = FileWork->selected_slot;
                card_request.buffer = (int)FileCardDataBuf;
                FileSaveDataPush();
                gauge.x = 128;
                gauge.y = FILE_GAUGE_Y;
                gauge.vertex_attribute = 0xffffffffU;
                gauge.width = FILE_GAUGE_WIDTH;
                gauge.height = FILE_GAUGE_HEIGHT;
                gauge.current = 0;
                gauge.maximum = FILE_CREATE_STEPS;
                gauge.filled_color[0] = FILE_GAUGE_FILLED_RED;
                gauge.filled_color[1] = FILE_GAUGE_FILLED_GREEN;
                gauge.filled_color[2] = FILE_GAUGE_FILLED_BLUE;
                gauge.filled_color[3] = FILE_GAUGE_ALPHA;
                gauge.unfilled_color[0] = FILE_GAUGE_UNFILLED;
                gauge.unfilled_color[1] = FILE_GAUGE_UNFILLED;
                gauge.unfilled_color[2] = FILE_GAUGE_UNFILLED;
                gauge.unfilled_color[3] = FILE_GAUGE_ALPHA;
                xglMcRequest(((FileCardNumber *)&FileWork->card_number)->number, MC_REQUEST_CREATE,
                             &card_request,
                             &FileWork->directory_result);
                tyaDrawGauge(&gauge);
                break;
            }
        }
        if (FileWork->use_hdd) {
            if (FileWork->menu_substate == 0) {
                hdd_request.create.status = FileWork->selected_slot;
                hdd_request.create.transfer.bytes = FileCardDataBuf;
                hdd_request.create.dataSize = MC_SAVE_DATA_SIZE;
                FileSaveDataPush();
                create_result = xglHddMcCreate(&hdd_request.create);
                FileWork->directory_result = create_result;
                switch (create_result) {
                case 0:
                    FileWork->command = FILE_STATE_SAVE_WRITE;
                    break;
                case HDD_CREATE_TOO_MANY_SAVES:
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                    FileWork->command = FILE_STATE_ERROR_HDD_TOO_MANY_SAVES;
                    FileWork->dispatch_delay = FILE_DELAY_LONG;
                    break;
                case HDD_CREATE_TOO_MANY_FOLDERS:
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                    FileWork->command = FILE_STATE_ERROR_HDD_TOO_MANY_FOLDERS;
                    FileWork->dispatch_delay = FILE_DELAY_LONG;
                    break;
                case HDD_CREATE_NO_SPACE:
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                    FileWork->command = FILE_STATE_ERROR_HDD_NO_SPACE;
                    FileWork->dispatch_delay = FILE_DELAY_LONG;
                    break;
                default:
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                    FileWork->command = FILE_STATE_ERROR_REQUEST_FAILED;
                    FileWork->dispatch_delay = FILE_DELAY_LONG;
                    break;
                }
            } else {
                FileWork->menu_substate--;
            }
        } else {
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            switch (card_status) {
            default:
                FileWork->command = FILE_STATE_ERROR_REQUEST_FAILED;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            case MC_STATUS_BUSY:
                gauge.current = FileWork->directory_result;
                tyaDrawGauge(&gauge);
                break;
            case MC_STATUS_READY:
                if (FileWork->directory_result < 0) {
                    FileWork->command = FILE_STATE_ERROR_REQUEST_FAILED;
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                } else {
                    FileWork->command = FILE_STATE_SAVE_WRITE;
                }
                gauge.current = FILE_CREATE_STEPS;
                tyaDrawGauge(&gauge);
                break;
            }
        }
        break;
    }
    case FILE_STATE_SAVE_DONE: {
        if (FileWork->use_hdd) {
            if (PadData.half_2a & PAD_CIRCLE) {
                FileWork->command = FILE_STATE_SLOT_LIST;
                xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                FileWork->menu_substate = FILE_WAIT_FRAMES;
            }
        } else {
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            switch (card_status) {
            case MC_STATUS_READY:
                if (PadData.half_2a & PAD_CIRCLE) {
                    FileWork->command = FILE_STATE_SLOT_LIST;
                    xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                    FileWork->menu_substate = FILE_WAIT_FRAMES;
                }
                break;
            case MC_STATUS_BUSY:
                break;
            case MC_STATUS_NONE:
            default:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            }
        }
        break;
    }
    case FILE_STATE_LOAD_READ:
    {
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            if (FileWork->use_hdd) {
                FileWork->dispatch_delay = FILE_DELAY_LONG;
            } else {
                card_request.slot = FileWork->selected_slot;
                card_request.buffer = (int)FileSaveData;
                card_request.size = MC_SAVE_DATA_SIZE;
                memset(FileSaveData, 0, MC_SAVE_DATA_SIZE);
                xglMcRequest(FileWork->card_number, MC_REQUEST_LOAD, &card_request,
                             &FileWork->directory_result);
            }
        } else {
            if (FileWork->use_hdd) {
                hdd_request.load.card = FileWork->selected_slot;
                hdd_request.load.data = FileSaveData;
                hdd_request.load.size = MC_SAVE_DATA_SIZE;
                memset(FileSaveData, 0, MC_SAVE_DATA_SIZE);
                FileWork->directory_result = xglHddMcLoad(&hdd_request.load);
                if (FileWork->directory_result != 0) {
                    FileWork->command = FILE_STATE_ERROR_REQUEST_FAILED;
                    xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                    break;
                }
                *(FileSaveRecord *) SaveData = *FileSaveData;
                ((void (*)(SaveDataHeader *))FileVersionCheck)((SaveDataHeader *) SaveData);
                xglSoundEffectNormalID(FILE_SE_LOAD_DONE, 0);
                FileWork->command = FILE_STATE_LOAD_DONE;
            } else {
                card = &FileWork->card;
                card_status = card->status[FileWork->card_number];
                if (card_status != MC_STATUS_READY) {
                    if (card_status != MC_STATUS_BUSY) {
                        FileWork->command = FILE_STATE_ERROR_REQUEST_FAILED;
                        xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                    }
                    break;
                }
                *(FileSaveRecord *) SaveData = *FileSaveData;
                ((void (*)(SaveDataHeader *))FileVersionCheck)((SaveDataHeader *) SaveData);
                xglSoundEffectNormalID(FILE_SE_LOAD_DONE, 0);
                FileWork->command = FILE_STATE_LOAD_DONE;
            }
        }
        break;
    }
    case FILE_STATE_LOAD_DONE: {
        if (FileWork->use_hdd) {
            if (PadData.half_2a & PAD_CIRCLE) {
                FileWork->command = FILE_STATE_CLOSING;
                xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
            }
        } else {
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            switch (card_status) {
            case MC_STATUS_READY:
                if (PadData.half_2a & PAD_CIRCLE) {
                    FileWork->command = FILE_STATE_CLOSING;
                    xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                }
                break;
            case MC_STATUS_BUSY:
                break;
            case MC_STATUS_NONE:
            default:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            }
        }
        break;
    }
    case FILE_STATE_CARD_UNFORMATTED: {
        if (FileWork->use_hdd) {
            if (PadData.half_2a & PAD_CIRCLE) {
                FileWork->command = FILE_STATE_CANCEL_CONFIRM;
                xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
            }
        } else {
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            switch (card_status) {
            case MC_STATUS_UNFORMATTED:
                if (PadData.half_2a & PAD_CIRCLE) {
                    xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                    FileWork->command = FILE_STATE_FORMAT_CONFIRM;
                }
                break;
            case MC_STATUS_NONE:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            case MC_STATUS_BUSY:
                break;
            default:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            }
        }
        break;
    }
    case FILE_STATE_FORMAT_CONFIRM: {
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            FileWork->action_selection = FILE_CHOICE_NO;
        }
        card = &FileWork->card;
        card_status = card->status[FileWork->card_number];
        switch (card_status) {
        case MC_STATUS_UNFORMATTED:
            FileWork->action_selection =
                MenuSelectMove(FileWork->action_selection, FILE_CONFIRM_ROWS, 0);
            if (PadData.half_2a & PAD_CIRCLE) {
                if (FileWork->action_selection == FILE_CHOICE_YES) {
                    xglMcRequest(FileWork->card_number, MC_REQUEST_FORMAT, 0,
                                 &FileWork->available_blocks);
                    FileWork->command = FILE_STATE_FORMAT;
                } else {
                    FileWork->command = FILE_STATE_CANCEL_CONFIRM;
                }
                xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
            }
            if (PadData.half_2a & PAD_CROSS) {
                FileWork->command = FILE_STATE_SELECT_DEVICE;
                xglSoundEffectNormalID(FILE_SE_CANCEL, 0);
            }
            break;
        case MC_STATUS_NONE:
            FileWork->command = FILE_STATE_ERROR_NO_CARD;
            xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
            break;
        case MC_STATUS_BUSY:
            break;
        default:
            FileWork->command = FILE_STATE_ERROR_NO_CARD;
            xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
            break;
        }
        break;
    }
    case FILE_STATE_FORMAT: {
        card = &FileWork->card;
        card_status = card->status[FileWork->card_number];
        switch (card_status) {
        case MC_STATUS_READY:
            FileWork->command = FILE_STATE_FORMAT_DONE;
            xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
            break;
        case MC_STATUS_UNFORMATTED:
        case MC_STATUS_BUSY:
            break;
        default:
            FileWork->command = FILE_STATE_FORMAT_FAILED;
            xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
            break;
        }
        break;
    }
    case FILE_STATE_FORMAT_DONE: {
        if (FileWork->dispatch_delay == 0) {
            if (PadData.half_2a & PAD_CIRCLE) {
                FileWork->command = FILE_STATE_CHECK_DEVICE;
                FileWork->hide_selection = 1;
            }
            card = &FileWork->card;
            card_status = card->status[FileWork->card_number];
            switch (card_status) {
            case MC_STATUS_NONE:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            case MC_STATUS_READY:
            case MC_STATUS_BUSY:
                break;
            default:
                FileWork->command = FILE_STATE_ERROR_NO_CARD;
                xglSoundEffectNormalID(FILE_SE_BUZZER, 0);
                break;
            }
        }
        break;
    }
    case FILE_STATE_FORMAT_FAILED:
        if (FileWork->dispatch_delay != 0) {
            break;
        }
        if (PadData.half_2a & PAD_CIRCLE) {
            FileWork->command = FILE_STATE_CANCEL_CONFIRM;
            xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
        }
        break;
    case FILE_STATE_ERROR_REQUEST_FAILED:
    case FILE_STATE_ERROR_NO_CARD:
    case FILE_STATE_ERROR_CANNOT_ACCESS:
    case 0x93:
    case FILE_STATE_ERROR_HDD_TOO_MANY_SAVES:
    case FILE_STATE_ERROR_HDD_TOO_MANY_FOLDERS:
    case FILE_STATE_ERROR_HDD_NO_SPACE:
    case 0x97:
        if (FileWork->dispatch_delay != 0) {
            break;
        }
        if (PadData.half_2a & PAD_CIRCLE) {
            FileWork->command = FILE_STATE_CANCEL_CONFIRM;
            xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
        }
        break;
    case FILE_STATE_CANCEL_CONFIRM:
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            FileWork->action_selection = FILE_CHOICE_NO;
            FileWork->dispatch_delay = 8;
            xglHddCheck();
        }
        if (FileWork->dispatch_delay == 0) {
            FileWork->action_selection =
                MenuSelectMove(FileWork->action_selection, FILE_CONFIRM_ROWS, 0);
            if (PadData.half_2a & PAD_CIRCLE) {
                if (FileWork->action_selection == FILE_CHOICE_YES) {
                    FileWork->result = 1;
                    FileWork->command = FILE_STATE_CLOSING;
                    xglSoundEffectNormalID(FILE_SE_CANCEL, 0);
                } else {
                    FileWork->command = FILE_STATE_SELECT_DEVICE;
                    xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
                }
            }
        }
        break;
    case FILE_STATE_INSERT_CARD:
        if (FileWork->dispatch_delay != 0) {
            break;
        }
        if (PadData.half_2a & PAD_CIRCLE) {
            FileWork->command = FILE_STATE_SELECT_DEVICE;
            xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
        }
        break;
    case FILE_STATE_NO_CARD_SPACE:
        if (FileWork->dispatch_delay != 0) {
            break;
        }
        if (PadData.half_2a & PAD_CIRCLE) {
            FileWork->command = FILE_STATE_CANCEL_CONFIRM;
            xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
        }
        break;
    case FILE_STATE_NO_SAVE_DATA:
        if (FileWork->dispatch_delay != 0) {
            break;
        }
        if (PadData.half_2a & PAD_CIRCLE) {
            FileWork->command = FILE_STATE_CANCEL_CONFIRM;
            xglSoundEffectNormalID(FILE_SE_DECIDE, 0);
        }
        break;
    case FILE_STATE_CLOSING:
        if (FileWork->input_flags & FILE_INPUT_ENTER) {
            FileWork->menu_substate = FILE_WAIT_FRAMES;
        }
        if (FileWork->menu_substate == 0) {
            FileWork->command = FILE_STATE_DONE;
        } else {
            FileWork->menu_substate--;
        }
        break;
    default:
        break;
    }
    FileWork->input_flags &= 0xfd;
    xglFontDebugHex(0, 0x48, FileWork->state, 2);
    xglFontDebugHex(0, 0x50, FileWork->command, 2);
    xglFontDebugHex(0, 0x58, FileWork->input_flags, 2);
    xglFontDebugHex(0, 0x60, FileWork->menu_substate, 2);
    xglFontDebugPrintf(0, 0x68, shortNumberFormat,
                       FileWork->debug_value_11);
    xglFontDebugPrintf(0x10, 0x68, shortNumberFormat,
                       FileWork->debug_value_12);
    xglFontDebugPrintf(0, 0x70, shortNumberFormat,
                       FileWork->device_selection);
    xglFontDebugPrintf(0x10, 0x70, shortNumberFormat,
                       FileWork->debug_value_14);
    xglFontDebugPrintf(0, 0x78, shortNumberFormat,
                       FileWork->debug_value_16);
    xglFontDebugPrintf(0x10, 0x78, shortNumberFormat,
                       FileWork->debug_value_17);
    xglFontDebugPrintf(0, 0x80, shortNumberFormat,
                       FileWork->selected_panel);
    xglFontDebugPrintf(0x10, 0x80, shortNumberFormat,
                       FileWork->panel_count);
    xglFontDebugPrintf(0, 0x88, shortNumberFormat,
                       FileWork->scroll_direction);
    xglFontDebugPrintf(0x10, 0x88, shortNumberFormat,
                       FileWork->debug_value_1b);
    xglFontDebugPrintf(0, 0x90, shortNumberFormat,
                       FileWork->action_selection);
    xglFontDebugPrintf(0, 0x98, shortNumberFormat,
                       FileWork->selected_slot);
    xglFontDebugPrintf(0x10, 0x98, shortNumberFormat,
                       FileWork->slot_count);
    xglFontDebugPrintf(0x20, 0x30, memoryCardMarkerText);
    xglFontDebugHex(0x20, 0x38, FileWork->card.status[0], 2);
    xglFontDebugHex(0x20, 0x40, FileWork->card.status[1], 2);
    xglFontDebugPrintf(0x20, 0x50, wideNumberFormat,
                       FileWork->available_blocks);
    xglFontDebugPrintf(0x20, 0x58, wideNumberFormat,
                       FileWork->directory_result);
    xglFontDebugPrintf(0x20, 0x60, wideNumberFormat,
                       FileWork->hdd_result);
    xglFontDebugPrintf(0x20, 0x68, wideNumberFormat,
                       FileWork->request_result);
}
#undef HddCheckState
#undef FileWork
#undef FileObjectDataSystem
#undef FileObjectData

typedef struct EndBackTexRenderView {
    u8 unmodeled_00[0x14];
    u16 width;
    u8 unmodeled_16[0x2a];
} EndBackTexRenderView;
extern EndBackTexRenderView sRender;

void endBackTexDraw(char *color) {
    XglPacket *packet;
    char rgba[4];
    int i;

    packet = xglPacketGetCurrent();
    if (color == (char *)-1) {
        rgba[3] = 0x80;
        rgba[2] = 0x80;
        rgba[1] = 0x80;
        rgba[0] = 0x80;
    } else {
        for (i = 0; i < 4; i++) {
            rgba[i] = color[i];
        }
    }
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    *(u64 *)0x70000000 = 0xA08B400000008001;
    *(u64 *)0x70000008 = 0xE53531EEEE;
    *(u64 *)0x70000010 = 0;
    *(u64 *)0x70000018 = 0x3F;
    *(u64 *)0x70000020 = 0x131000000;
    *(u64 *)0x70000028 = 0x4E;
    *(u64 *)0x70000030 = (u64)(0x24020000 | (sRender.width << 5)) | 0x2000000640000000;
    *(u64 *)0x70000038 = 6;
    *(u64 *)0x70000040 = 0x31001;
    *(u64 *)0x70000048 = 0x47;
    *(int *)0x70000050 = rgba[0];
    *(int *)0x70000054 = rgba[1];
    *(int *)0x70000058 = rgba[2];
    *(int *)0x7000005C = rgba[3];
    *(int *)0x70000060 = 0;
    *(int *)0x70000064 = 0;
    *(u64 *)0x70000068 = 0;
    *(int *)0x70000070 = 0x6FF8;
    *(int *)0x70000074 = 0x71F8;
    *(u64 *)0x70000078 = 0;
    *(int *)0x70000080 = 0x2000;
    *(int *)0x70000084 = 0x1C00;
    *(u64 *)0x70000088 = 0;
    *(int *)0x70000090 = 0x8FF8;
    *(int *)0x70000094 = 0x8DF8;
    *(u64 *)0x70000098 = 0;
    *(u64 *)0x700000A0 = 0x31000000;
    *(u64 *)0x700000A8 = 0x4E;
    sceVif1PkAddDirectDataN(packet, (void *)0x70000000, 11);
    sceVif1PkCloseDirectHLCode(packet);
}

#define NULL 0
void MenuFilePas(TskObject *task, void *data);
void MenuFileInfo(TskObject *task, void *data);
void tskFileSelect(TskObject *task, void *data);
void tskFileData(TskObject *task, void *data);
extern u8 *FileTask;
extern void *FileCardDataBuf;
extern u8 *FileExistCheckWork;
extern void *xglTaskInitial(void *pool, int capacity, int flags);
extern void MenuWorkEndCheck(unsigned char *p);
extern void MenuFile(void);
extern int GameSnapShotSaveThumbnail(int quality, unsigned int buffer);
void endPrintDirectFrameCopy(void *, int, int);
void endPrintInit(void);
extern const char D_004C4D80[];
extern const char D_004C4D98[];
extern const char D_004C4DB0[];
extern const char D_004C4DC8[];
typedef struct { int pad[4]; int flags; } MfmLoopState;
typedef struct { unsigned char pad0[0x1007]; unsigned char a; unsigned char b; unsigned char pad1[0x47]; long long c; unsigned char pad2[8]; } MfmSlot;
extern void exit(int) __attribute__((noreturn));
extern void GameSnapShotCheck(void);

int MenuFileMain(int arg0)
{
    int heap;
    void *work;
    XglTaskPrefix *tail;
    int i;
    int n;
    int off;
    int tmp;
    char *srp;
    unsigned char *p;
    unsigned char *q;
    unsigned char *r;

    srp = (char *)&sRender;
    endPrintDirectFrameCopy(xglPacketGetCurrent(), *(unsigned short *)(srp + 0x10) << 5, *(unsigned short *)(srp + 0x12) << 5);
    ((MfmLoopState *)GameLoopState)->flags = ((MfmLoopState *)GameLoopState)->flags | 0x04000000;
    xglSleep();
    endPrintInit();
    ((MfmLoopState *)GameLoopState)->flags = ((MfmLoopState *)GameLoopState)->flags | 0x04000000;
    MenuCfTaikiPush();
    endPrintDirectFrameCopy(xglPacketGetCurrent(), *(unsigned short *)(srp + 0x12) << 5, *(unsigned short *)(srp + 0x14) << 5);
    endBackTexDraw((char *)-1);
    xglSleep();
    heap = (MenuWorkEndGet() + 0x7F) & ~0x7F;
    work = (char *)heap + 0x80;
    FileWork = (FileWorkBlock *)heap;
    memset((void *)heap, 0, 0x80);
    work = MenuBgTaskInit((unsigned char *)work, 1);
    FileTask = work;
    work = xglTaskInitial(work, 0x10, 0);
    TskObjectSet((TskObject *)xglTaskEntryNext((XglTaskScheduler *)FileTask, (void (*)(XglTaskPrefix *))tskTskMain, FileTask != NULL ? *(XglTaskPrefix **)(FileTask + 8) : NULL), MenuFilePas, work);
    work = (char *)work + 0x3CC;
    TskObjectSet((TskObject *)xglTaskEntryNext((XglTaskScheduler *)FileTask, (void (*)(XglTaskPrefix *))tskTskMain, FileTask != NULL ? *(XglTaskPrefix **)(FileTask + 8) : NULL), MenuFileInfo, work);
    work = (char *)work + 0x460;
    TskObjectSet((TskObject *)xglTaskEntryNext((XglTaskScheduler *)FileTask, (void (*)(XglTaskPrefix *))tskTskMain, FileTask != NULL ? *(XglTaskPrefix **)(FileTask + 8) : NULL), tskFileSelect, work);
    work = (char *)work + 0x1E8C;
    TskObjectSet((TskObject *)xglTaskEntryNext((XglTaskScheduler *)FileTask, (void (*)(XglTaskPrefix *))tskTskMain, FileTask != NULL ? *(XglTaskPrefix **)(FileTask + 8) : NULL), (void (*)(TskObject *, void *))tskFileEx, work);
    work = (char *)work + 0x50;
    tail = (XglTaskPrefix *)xglTaskEntryNext((XglTaskScheduler *)FileTask, (void (*)(XglTaskPrefix *))tskTskMain, FileTask != NULL ? *(XglTaskPrefix **)(FileTask + 8) : NULL);
    tmp = 0x01000000;
    TskObjectSet((TskObject *)tail, tskFileData, work);
    FileCardDataBuf = (void *)tmp;
    tmp += 0x12000;
    FileObjectDataSystem = (unsigned char *)tmp;
    work = (char *)work + 0x249C;
    memset((void *)tmp, 0, 0x258);
    i = 0;
    do {
        FileObjectDataSystem[i + 4] = -1;
        i++;
    } while (i < 0x63);
    FileJpegDec = (unsigned char *)(tmp + 0x800);
    memset(FileJpegDec, 0, 0x46500);
    FileObjectData = (unsigned char *)(tmp + 0x47000);
    memset(FileObjectData, 0, 0x696A0);
    tmp += 0xB06A0;
    p = FileObjectData;
    for (n = 0; n < 3; n++) {
        r = (unsigned char *)(n * 0x1060 + (int)p);
        p = r;
        p += 0x65520;
        p[0x1007] = -1;
        q = (unsigned char *)(n * 0x1060 + (int)FileObjectData);
        q += 0x65520;
        q[0x1008] = -1;
        p = FileObjectData;
        q = (unsigned char *)(n * 0x1060 + (int)p);
        q += 0x65520;
        *(long long *)(q + 0x1050) = -1;
    }
    FileSaveData = (FileChecksumData *)((tmp + 0x7FF) & ~0x7FF);
    FileExistCheckWork = (((tmp + 0x7FF) & ~0x7FF) + 0x16BAF) & ~0x7FF;
    memset((void *)FileExistCheckWork, 0, 0x2000);
    if (arg0 == 0) {
        memset(FileObjectData + 0x68644, 0, 0x1000);
        if (GameSnapShotSaveThumbnail(0x1E, (unsigned int)(FileObjectData + 0x68644)) >= 0x1001) {
            exit(1);
        }
    }
    MenuLoadInit();
    MenuFontLoad(1, 1);
    WindowTexLoad(NULL, 0x10001);
    WindowTexLoad(NULL, 0x10002);
    work = (void *)MenuSaveMapTextLoad((int)work, 1);
    if (arg0 == 0) {
        MenuLoadFile(D_004C4D80, FileCardDataBuf);
    }
    MenuLoadFile(D_004C4D98, FileObjectData + 0x65524);
    MenuLoadFile(D_004C4DB0, FileObjectData + 0x66584);
    MenuLoadFile(D_004C4DC8, FileObjectData + 0x675E4);
    MenuWorkEndCheck(work);
    if (arg0 == 0) {
        xglSoundEffectNormalID(1, 0);
    }
    goto loop_23_test;
loop_23_body:
    MenuBgTaskMain();
    xglSleep();
loop_23_test:
    if (MenuLoadSync() != 0) {
        goto loop_23_body;
    }
    FileWork->unmodeled_00[2] = arg0;
    FileWork->unmodeled_00[1] = 0x24;
    FileWork->unmodeled_00[0] = 2;
    FileWork->unmodeled_04[1] = 0x00;
    while (GameSnapShotCheck(), FileWork->state != (unsigned char)0xFF && (MenuFile(), 1)) {
        endPrintExtFunc(0, 0x64, NULL);
        MenuBgTaskMain();
        xglTaskExecute((XglTaskScheduler *)FileTask);
        xglSleep();
    }
    MenuBgTaskBreak();
    xglTaskExecute((XglTaskScheduler *)FileTask);
    MenuCfTaikiPop();
    MenuFontLoad(0, 0);
    WindowTexLoad(NULL, 0);
    if (arg0 == 0) {
        GameCfPlayerLoadResource(0);
    }
    MapChangeFadeSet();
    ((MfmLoopState *)GameLoopState)->flags &= 0xFBFFFFFF;
    return FileWork->unmodeled_04[1] == 0;
}

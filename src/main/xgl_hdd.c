#include "common.h"
#include "shared.h"
#include "main/xgl_hdd.h"
#include "xgl_hdd.h"

static int xglHddDummyCB(int event, int value)
{
    return 0;
}

static int xglHddCheckCore(void)
{
    long status;
    int expectedStatus;
    int result;

    if (HddActive >= 2)
        return -5;

    status = sceDevctl(xgl_hdd_device, 0x4807, 0, 0, 0, 0);
    if (status != 0) {
        if (status == 2)
            return -2;
        if (status < 3) {
            expectedStatus = 1;
            result = -3;
        } else {
            expectedStatus = 3;
            result = -1;
        }
        if (status != expectedStatus)
            return -99;
    } else {
        if (sceDevctl(xgl_hdd_device, 0x4804, 0, 0, 0, 0) >= 0)
            return 0;
        if (HddActive == 1)
            return -6;
        xglHddActivate(0x102);
        return -5;
    }
    return result;
}

int xglHddCheck2(void)
{
    int result = xglHddCheckCore();

    if (result == -6) {
        xglHddActivate(0x103);
        return -5;
    }
    return result;
}

int xglHddCheck(void)
{
    int result;

    if (HddActive == 3) {
        xglHddErrorScreen();
        xglHddActivate(0);
        xglHddActivate(0x102);
    }

    result = xglHddCheckCore();
    if (result != -6)
        return result;

    xglHddErrorScreen();
    xglHddActivate(0);
    xglHddActivate(0x102);
    return -5;
}

void xglHddErrorScreen(void)
{
    int waitFrames = 30;

    sRender.errorScreenActive = 1;
    for (;;) {
        xglDmaDirectNormal(2, TestEnv_0_004A8A80, 6);
        xglFontPrint(48, 64, 0xffffff, D_004D2628);
        if (waitFrames > 0) {
            waitFrames--;
        } else if (PadData.half_2a & 0x20) {
            break;
        }
        xglSleep();
    }
    xglSoundEffectNormalDirect(1);
    sRender.errorScreenActive = 0;
}

int xglHddMcLoadMount(void)
{
    int result;

    if (xglHddCheck2() != 0)
        return -1;

    result = sceMount((char *)hdd_mc_path, (char *)commonname, 5, 0, 0);
    if (result < 0 && result != -16)
        return -1;
    return 0;
}

int xglHddMcUmount(void)
{
    int result = sceUmount(hdd_mc_path);
    return result < 0 ? -2 : 0;
}

static char *make_fullpath(char *destination, int card, int slot)
{
    const char *source = yoursaves;

    while ((*destination = *source) != 0) {
        source++;
        destination++;
    }

    source = xglMcSetFullPath(card, slot);
    while ((*destination = *source) != 0) {
        source++;
        destination++;
    }
    return destination;
}

int xglHddMcExist(struct HddExistRequest *request)
{
    char filePath[256];
    char name[256];
    struct HddDirectoryEntry entry;
    char savePath[256];
    XglClock timestamp;
    int result;
    int descriptor;
    int save;
    int count;
    int newestSave;
    unsigned int newestTime;
    unsigned int time;
    unsigned char *found;
    unsigned char *source;
    char *destination;
    unsigned char *clearPosition;

    result = xglHddMcLoadMount();
    if (result < 0)
        return result;

    if (request->card >= 0) {
        make_fullpath(filePath, request->card, 0);
        descriptor = sceOpen(filePath, 1, 438);
        if (descriptor >= 0) {
            result = sceClose(descriptor) < 0 ? -3 : 1;
        } else {
            result = 0;
        }
    } else {
        found = request->found;
        source = xglMcSetFullPath(-1, -1);
        source++;
        newestTime = 0;
        newestSave = 0;
        destination = name;
        *destination = *source;
        if (((unsigned int)*destination << 24) != 0) {
            do {
                source++;
                destination++;
                *destination = *source;
            } while (((unsigned int)*destination << 24) != 0);
        }
        /* The -1 card path ends in a wildcard; compare its directory stem. */
        destination[-1] = 0;

        count = 100;
        clearPosition = &found[100];
        do {
            count--;
            *clearPosition = 0;
            clearPosition--;
        } while (count >= 0);

        count = 0;
        descriptor = sceDopen(yoursaves);
        while (descriptor >= 0 && sceDread(descriptor, &entry) > 0) {
            if (strncmp(name, entry.name, 16) != 0)
                continue;
            save = entry.name[16] * 10 + entry.name[17] - (10 * '0' + '0');
            make_fullpath(savePath, save, 0);
            if (sceGetstat(savePath, &entry.stat) < 0)
                continue;
            found[save] = 1;
            timestamp.status = 0;
            timestamp.year = entry.stat.year;
            timestamp.month = entry.stat.month;
            timestamp.day = entry.stat.day;
            timestamp.hour = entry.stat.hour;
            timestamp.minute = entry.stat.minute;
            timestamp.second = entry.stat.second;
            time = xglClockDayTime2UInt(&timestamp);
            if (newestTime < time) {
                newestTime = time;
                newestSave = save;
            }
            count++;
        }
        found[100] = newestSave;
        result = count;
        if (descriptor >= 0 && sceDclose(descriptor) < 0)
            result = -5;
    }
    xglHddMcUmount();
    return result;
}

int xglHddMcLoadCore(void *save)
{
    struct HddLoadRequest *request = save;
    char path[256];
    int descriptor;
    int result;

    make_fullpath(path, request->card, 0);
    descriptor = sceOpen(path, 1, 438);
    if (descriptor < 0) {
        result = descriptor == -2 ? 1 : -6;
    } else {
        result = 0;
        if (sceRead(descriptor, request->data, request->size) < 0)
            result = -7;
        if (sceClose(descriptor) < 0)
            result = -8;
    }
    return result;
}

int xglHddMcLoad(void *save)
{
    int result;

    result = xglHddMcLoadMount();
    if (result < 0)
        return result;
    result = xglHddMcLoadCore(save);
    if (result < 0)
        return result;
    return xglHddMcUmount();
}

extern int strcmp(const char *left, const char *right);
extern char D_004DC378[];
extern char D_004DC380[];
extern char D_004DC388[];

static int Judge_MakeNewFolder(void)
{
    struct HddDirectoryEntry entry;
    int descriptor;
    int result;
    int count;
    int i;

    count = 0;
    result = sceDopen(D_004DC378);
    if (result < 0)
        return result;
    descriptor = result;
    for (i = 0; i < 0x400; i++) {
        result = sceDread(descriptor, &entry);
        if (result <= 0)
            break;
        if (strcmp(entry.name, D_004DC380) == 0 || strcmp(entry.name, D_004DC388) == 0)
            continue;
        if ((((struct HddIoStat *)&entry.stat)->mode & 0xF000) == 0x1000
            && ((struct HddIoStat *)&entry.stat)->privateData[0] == 0xFFFF
            && ((struct HddIoStat *)&entry.stat)->privateData[1] == 0xFFFF) {
            count++;
            if (count >= 0x100)
                break;
        }
    }
    {
        int closed = sceDclose(descriptor);
        if (result >= 0)
            result = closed;
    }
    if (result < 0)
        return result;
    return count < 0x100;
}

extern unsigned int strlen(const char *string);
extern char *strchr(const char *string, int c);

int Judge_MakeNewSavedata(int descriptor, int card)
{
    struct HddDirectoryEntry entry;
    int result;
    int count;
    int i;
    char *prefix;
    char *p;
    int length;

    result = 0;
    count = 0;
    prefix = (char *)xglMcSetFullPath(card, -1) + 1;
    if (card < 0) {
        p = prefix;
        while (*p != 0)
            p++;
        p[-1] = 0;
    }
    length = strlen(prefix);
    for (i = 0; i < 0x400; i++) {
        result = sceDread(descriptor, &entry);
        if (result <= 0)
            break;
        if (strcmp(entry.name, D_004DC380) == 0 || strcmp(entry.name, D_004DC388) == 0)
            continue;
        if (strncmp(entry.name, prefix, length) == 0)
            continue;
        if ((((struct HddIoStat *)&entry.stat)->mode & 0xF000) == 0x1000
            && (((struct HddIoStat *)&entry.stat)->attributes & 0x4000) == 0)
            continue;
        if (strchr(entry.name, ':') != 0
            && (((struct HddIoStat *)&entry.stat)->mode & 0xF000) == 0x2000)
            continue;
        if ((((struct HddIoStat *)&entry.stat)->mode & 0xF000) == 0x4000)
            continue;
        if (((struct HddIoStat *)&entry.stat)->privateData[0] == 0xFFFF
            && ((struct HddIoStat *)&entry.stat)->privateData[1] == 0xFFFF)
            count++;
    }
    {
        int closed = sceDclose(descriptor);
        if (result >= 0)
            result = closed;
    }
    if (result < 0)
        return result;
    return count < 0x3FE;
}

static int xglHddMcCheckYourSaves(int card)
{
    int descriptor = sceDopen(yoursaves);

    if (descriptor < 0)
        return Judge_MakeNewFolder() == 1 ? 1 : -2;
    return Judge_MakeNewSavedata(descriptor, card) == 1 ? 0 : -3;
}

static int xglHddMcCheckCore(struct HddCheckState *state)
{
    int result = xglHddMcCheckYourSaves(state->status);

    if (result < 0)
        return result;
    {
        int clusterSize = sceDevctl(hdd_mc_path, 0x5001, 0, 0, 0, 0);
        int freeClusters = sceDevctl(hdd_mc_path, 0x5002, 0, 0, 0, 0);
        int dataClusters = (state->dataSize + clusterSize - 1) / clusterSize;
        int reserveClusters = ((unsigned int)clusterSize + 963) / (unsigned int)clusterSize;
        int transferClusters =
            ((unsigned int)(state->transfer->end - state->transfer->begin) + clusterSize - 1)
            / (unsigned int)clusterSize;

        dataClusters += reserveClusters;
        if (freeClusters < dataClusters + transferClusters + 7) {
            int capacity;

            if (sceDevctl((char *)xgl_hdd_device, 0x480a, 0, 0, &capacity, 4) < 0)
                return -1;
            return capacity <= 0x1fffff ? -4 : 1;
        }
    }
    return 0;
}

int xglHddMcCheck(struct HddCheckState *state)
{
    int previous;
    int result;

    if (xglHddCheck2() != 0)
        return -1;

    xglHddMcUmount();
    if (sceMount(hdd_mc_path, commonname, 4, 0, 0) < 0)
        return -1;

    previous = state->status;
    state->status = -1;
    result = xglHddMcCheckCore(state);
    state->status = previous;
    xglHddMcUmount();
    return result;
}

int xglHddMcGetFree(void)
{
    int result;
    int freeSpace;
    int clusterSize;
    int freeClusters;
    int capacity;

    result = xglHddMcLoadMount();
    if (result >= 0) {
        freeSpace = xglHddMcCheckYourSaves(-1);
        if (freeSpace >= 0) {
            if (sceDevctl(xgl_hdd_device, 0x480A, 0, 0, &capacity, 4) == 0
                && capacity > 0x1FFFFF) {
                freeSpace = 0x100000;
            } else {
                clusterSize = sceDevctl(hdd_mc_path, 0x5001, 0, 0, 0, 0);
                freeClusters = sceDevctl(hdd_mc_path, 0x5002, 0, 0, 0, 0);
                freeSpace = (freeClusters < 0 ? 0 : freeClusters) * (clusterSize / 1024);
            }
        }
        xglHddMcUmount();
        result = freeSpace;
    }
    return result;
}

static int create_file(int card, int slot, const void *data, int size)
{
    char path[256];
    struct HddIoStat stat;
    int descriptor;
    int result;

    make_fullpath(path, card, slot);
    descriptor = sceOpen(path, 0x602, 438);
    if (descriptor < 0)
        return -1;

    result = 0;
    if (sceWrite(descriptor, data, size) < 0)
        result = -1;
    if (sceClose(descriptor) < 0)
        result = -1;
    stat.attributes = 0x8497;
    if (sceChstat(path, &stat, 2) < 0)
        result = -1;
    return result;
}

extern int sceIoctl2(int handle, int command, const void *input, unsigned int input_size,
                     void *output, unsigned int output_size);
extern int sceMkdir(const char *path, int mode);
extern void xglMcWriteMapName(char *buffer, int slot);
extern unsigned char tbl_1_004A8AE0[];
extern char D_004DC390[];

int xglHddMcCreate(struct HddCheckState *state)
{
    char path[256];
    struct HddIoStat stat;
    struct HddTransferInfo *transfer;
    int descriptor;
    int result;
    int ret;

    ret = -1;
    xglHddMcUmount();
    if (sceMount(hdd_mc_path, commonname, 4, 0, 0) < 0)
        return -1;
    make_fullpath(path, state->status, -1);
    if (state->status >= 0 && (descriptor = sceDopen(path)) >= 0) {
        if (sceDclose(descriptor) < 0) {
            ret = -0x15;
            goto end;
        }
    } else {
        result = xglHddMcCheckCore(state);
        if (result < 0) {
            ret = -tbl_1_004A8AE0[~result];
            goto end;
        }
        if (result == 1) {
            if (xglHddMcUmount() < 0)
                return -0x19;
            descriptor = sceOpen(commonname, 3);
            if (descriptor < 0)
                return -0x10;
            ret = 0;
            if (sceIoctl2(descriptor, 0x6801, D_004DC390, 3, 0, 0) < 0)
                ret = -0x11;
            if (sceClose(descriptor) < 0)
                ret = -0x12;
            if (ret != 0)
                return ret;
            if (sceMount(hdd_mc_path, commonname, 4, 0, 0) < 0)
                return -1;
        }
        descriptor = sceDopen(yoursaves);
        if (descriptor < 0) {
            if (sceMkdir(yoursaves, 0x1FF) < 0) {
                ret = -0xA;
                goto end;
            }
        } else if (sceDclose(descriptor) < 0) {
            ret = -0xC;
            goto end;
        }
        if (state->status < 0)
            goto end;
        descriptor = sceDopen(path);
        if (descriptor < 0) {
            ret = -0x13;
            if (descriptor != -2)
                goto end;
        } else if (descriptor != -2) {
            goto close;
        }
        if (sceMkdir(path, 0x1FF) < 0) {
            ret = -0x14;
            goto end;
        }
        stat.attributes = 0xC4A7;
        if (sceChstat(path, &stat, 2) < 0) {
            ret = -0x18;
            goto end;
        }
        goto write;
    close:
        ret = -0x15;
        if (sceDclose(descriptor) < 0)
            goto end;
    }
write:
    transfer = state->transfer;
    ret = -0x16;
    xglMcWriteMapName((char *)transfer + 0x40, state->status);
    if (create_file(state->status, 1, (char *)transfer + 0x40, 0x3C4) >= 0) {
        ret = -0x17;
        if (create_file(state->status, 2, (char *)transfer + 0x440, transfer->end - transfer->begin) >= 0)
            ret = 0;
    }
end:
    xglHddMcUmount();
    return ret;
}

int xglHddMcSave(const struct HddSaveRequest *save)
{
    int result;

    if (xglHddCheck2() != 0)
        return -1;
    if (sceMount(hdd_mc_path, commonname, 4, 0, 0) < 0)
        return -1;

    result = create_file(save->card, 0, save->data, save->size) < 0 ? -24 : 0;
    xglHddMcUmount();
    return result;
}

int xglHddUninstall(void)
{
    int result;

    xglHddActivate(0);
    result = sceUmount(mount_device);
    if (result < 0 && result != -19)
        return -1;
    result = sceRemove(partitionname);
    return result < 0 ? -5 : 0;
}

static int xglHddInstallReadCB(int event, int value)
{
    HddInstallCBParam *cb;
    int result;
    int total;
    int scaledValue;
    int offset;

    cb = HddInstallCBparam;
    if (event == 1) {
        cb->total = value + 1;
        return 1;
    }

    result = 2;
    if (event == 2) {
        total = cb->total;
        scaledValue = value * 0x10;
        offset = cb->base * 0x10 + scaledValue / total;
        result = cb->callback(6, offset, cb->param);
        cb->status = result;
        if (result != 0) {
            return xglCdReadCancel();
        }
    }
    return result;
}

extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
extern int xglCdReadFilePart(const char *name, void *buffer, int mode,
                             void *callback, int offset, int length);
extern int xglCdGetFileSize(const char *name);
extern int sceFormat(char *device, char *block_device, void *argument, int argument_size);
extern char D_004D26C8[];
extern char D_004D26D8[];
extern char D_004DC3A0[];
extern char D_004DC3A8[];
extern char dvdname_2[];
extern char hddname[];

int xglHddInstall(int (*callback)(int, int, int), int param)
{
  HddInstallCBParam ctx;
  char path[256];
  int format_arg[4];
  int result;
  int code;
  int fd;
  int fd2;
  int size;
  int base;
  int offset;
  int chunk;
  unsigned char *dst;
  int i;
  if (callback == 0)
  {
    callback = xglHddDummyCB;
  }
  HddInstallCBparam = &ctx;
  ctx.callback = callback;
  ctx.param = param;
  ctx.status = 0;
  callback(0, 0, param);
  if (xglCdReadFile(D_004D26C8, (void *) 0x1000000, 0, 1) < 0)
  {
    callback(-1, 0, param);
    return -1;
  }
  {
    int k;
    unsigned char *src = (unsigned char *) system_cnf;
    for (k = 0, dst = (unsigned char *) 0x1000200; k < 0x50; k++)
    {
      *(dst++) = src[k];
    }

  }
  size = (((*((int *) 0x1000028)) + (*((int *) 0x100002C))) + 0x1FF) & (~0x1FF);
  callback(1, 0, param);
  sprintf(path, D_004D26D8, partitionname);
  fd = sceOpen(path, 0x203);
  if (fd < 0)
  {
    callback(-2, fd, param);
    return -1;
  }
  callback(2, 0, param);
  result = sceWrite(fd, (void *) 0x1000000, size);
  if (result < 0)
  {
    callback(-3, result, param);
    xglHddUninstall();
    return -1;
  }
  size = 0x80000;
  i = 1;
  do
  {
    callback(3, i++, param);
    result = sceIoctl2(fd, 0x6801, D_004DC3A0, 5, 0, 0);
    if (result < 0)
    {
      callback(-4, result, param);
      xglHddUninstall();
      return -1;
    }
    size += 0x80000;
  }
  while (size < 0x380000);
  result = sceClose(fd);
  if (result < 0)
  {
    callback(-5, result, param);
    return -1;
  }
  callback(4, 0, param);
  format_arg[0] = 0x2000;
  result = sceFormat(D_004DC3A8, partitionname, format_arg, 4);
  if (result < 0)
  {
    callback(-6, result, param);
    xglHddUninstall();
    return -1;
  }
  callback(5, 0, param);
  result = sceMount((char *) mount_device, partitionname, 4, 0, 0);
  if (result < 0)
  {
    callback(-7, result, param);
    xglHddUninstall();
    return -1;
  }
  callback(6, 0, param);
  fd2 = sceOpen(hddname, 0x602, 0x1B6);
  if (fd2 < 0)
  {
    result = fd2;
    callback(-8, result, param);
    xglHddUninstall();
    return -1;
  }
  base = 0;
  dvdname_2[10] = '0';
  while ((size = xglCdGetFileSize(dvdname_2)) >= 0)
  {
    base += (size + 0xFFFFFF) / 0x1000000;
    dvdname_2[10]++;
  }

  base <<= 16;
  dvdname_2[10] = '0';
  while ((size = xglCdGetFileSize(dvdname_2)) >= 0)
  {
    for (offset = 0; offset < size;)
    {
      chunk = size - offset;
      if (chunk > 0x1000000)
      {
        chunk = 0x1000000;
      }
      ctx.base = base++;
      xglCdReadFilePart(dvdname_2, (void *) 0x1000000, 2, xglHddInstallReadCB, offset, chunk);
      offset += chunk;
      if (ctx.status != 0)
      {
        sceClose(fd2);
        if (xglHddCheck() == 0)
        {
          xglHddUninstall();
          return 1;
        }
        return (ctx.status == 1) ? (1) : (-1);
      }
      result = sceWrite(fd2, (void *) 0x1000000, chunk);
      if (result != chunk)
      {
        callback(-9, result, param);
        sceClose(fd2);
        if (xglHddCheck() == 0)
        {
          xglHddUninstall();
        }
        return -1;
      }
    }

    dvdname_2[10]++;
  }

  result = sceClose(fd2);
  if (result < 0)
  {
    callback(-10, result, param);
    xglHddUninstall();
    return -1;
  }
  callback(7, 0, param);
  fd2 = sceOpen((char *) hddcheck, 0x602, 0x1B6);
  if (fd2 < 0)
  {
    result = fd2;
    callback(-11, result, param);
    xglHddUninstall();
    return -1;
  }
  callback(7, 1, param);
  xglCdGetFileData((char *) cd_filename, (void *) 0x1000000);
  result = sceWrite(fd2, (void *) 0x1000000, 0x200);
  if (result != 0x200)
  {
    callback(-12, result, param);
    sceClose(fd2);
    xglHddUninstall();
    return -1;
  }
  callback(7, 2, param);
  result = sceClose(fd2);
  if (result < 0)
  {
    callback(-13, result, param);
    xglHddUninstall();
    return -1;
  }
  callback(7, 3, param);
  result = sceUmount((char *) mount_device);
  if (result < 0)
  {
    callback(-14, result, param);
    xglHddUninstall();
    return -1;
  }
  callback(7, 4, param);
  result = sceMount((char *) mount_device, partitionname, 5, 0, 0);
  if (result < 0)
  {
    callback(-15, result, param);
    xglHddUninstall();
    return -1;
  }
  callback(8, 0, param);
  return 0;
}

int xglHddActivate(int state)
{
    int previous = HddActive;

    if (state != -1) {
        if (previous != 2 && (state & 0xff) < 4 && (state & 0xff) >= 0) {
            HddActive = (u8)state;
            if (state < 0x100)
                xglCdArcCheck();
        }
    }
    return previous < 2 ? previous : 0;
}

static void decrypt(void)
{
    u8 *source = partitionname;
    int value = *source;
    register int masked = value & 0xff;
    register u8 *destination = source;

    if (masked != 0x68) {
        if (masked != 0) {
            do {
                *destination = (unsigned int)value ^ 0x80;
                source++;
                value = *source;
                destination++;
            } while (value != 0);
        }
    }
    partitionname[17] = system_cnf[0x1a];
}

int xglHddMount(void)
{
    int mount_result;
    register int readiness;
    int device_result;
    int close_result;
    int capacity;
    int descriptor;
    int mismatch;
    int index;
    u8 *buffer;
    u8 *reference;

    decrypt();
    if (HddActive >= 2)
        return -1;

    HddActive = 0;
    readiness = xglHddCheck();
    if (readiness != 0)
        return readiness == -5 ? -1 : readiness;

    mount_result = sceMount((char *)mount_device, partitionname, 5, 0, 0);
    if (mount_result == -5)
        return mount_result;
    if (mount_result < 0 && mount_result != -16) {
        if (mount_result == -2) {
            device_result = sceDevctl((char *)xgl_hdd_device, 0x480a, 0, 0,
                                      &capacity, 4);
            if (device_result < 0)
                return -2;
            if (capacity <= 0x37ffff)
                return -4;
            device_result = sceDevctl((char *)xgl_hdd_device, 0x4801, 0, 0, 0, 0);
            if (device_result <= 0x7ffff)
                return -4;
            return 0;
        }
        return -2;
    }

    descriptor = sceOpen((char *)hddcheck, 1, 0x16d);
    if (descriptor < 0)
        return -5;

    buffer = (u8 *)(((u32)WorkEnd + 63) & (u32)-64);
    reference = buffer + 512;
    buffer[0] = 1;
    reference[0] = 2;
    mismatch = 0;
    if (sceRead(descriptor, buffer, 512) < 0) {
        mismatch = 1;
    } else {
        xglCdGetFileData((char *)cd_filename, reference);
        for (index = 0; index < 36; index++) {
            if (buffer[index] != reference[index]) {
                mismatch = 1;
                break;
            }
        }
    }
    close_result = sceClose(descriptor);
    if (close_result < 0 || mismatch == 1)
        return -5;
    HddActive = 1;
    return 1;
}

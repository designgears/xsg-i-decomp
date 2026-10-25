#include "common.h"
#include "shared.h"
#include "main/xgl_cd.h"
#include "main/xgl_thread.h"
#include "xgl_cd.h"

/* Types recovered upstream (origin/master); kept for reference. */
/* Four 0x18-byte archive slots; the initialization helper models each prefix. */
typedef union CdArchiveStorage {
    CdArchiveEntry entry;
    unsigned char bytes[0x18];
} CdArchiveStorage;


/*
 * File-local functions (LOCAL in the original symbol table) that the shared
 * header declares `extern`: declare them static first so their static
 * definitions below do not follow a non-static declaration.
 */
static int BCD2INT(unsigned char x);

/* extract (below) is still INCLUDE_ASM; declare it so its callers do not see
 * an implicit declaration. */
static int extract(void *destination, void *source, int byte_count);

extern int sceCdStRead(int sectors, void *buffer, int mode, int *result);
extern int sceRead(int descriptor, void *buffer, int bytes);
extern const char D_004DC2D0[];
extern const char D_004DC2D8[];
extern const char D_004DC2E0[];

/* StrList holds a single global stream slot; queue_next's stream scan below
 * walks it as a one-entry list, as xglCdStreamClose already does. */
#define STR_LIST_LAST_INDEX 0

/*
 * queue_next and xglCdControlThread share LW through this typed view.
 * LW is the shared, absolutely-addressed unsigned-byte array (hdd_error's
 * comment above), so the cast cannot move into LW's own declaration without
 * retyping it for every other user of this TU. A per-function local variable
 * initialized once from this cast was tried and measured to change cc1
 * 2.96's register allocation/scheduling for both functions below (fewer
 * callee-saved registers, reordered address arithmetic), so the cast stays
 * this one named, TU-local macro instead of a declared object.
 */
#define CD_READ_CONTROL ((CdReadControl *)LW)

/* Twelve month lengths followed by twelve cumulative month offsets. */
extern unsigned short monthday[24];

static int StreamReadRingCoreSub(CdStreamParam *stream, void *buffer, int sectors);
static void StreamReadRingCoreNormal(CdStreamParam *stream);
static void StreamReadRingCoreXss(CdStreamParam *stream);

typedef void CdCompletionCallback(int event, int value);

/*
 * The queue entries occupy 0x80 bytes; only their used prefix is named.
 * xglCdReadFilePart (0x0021DEB8..0x0021E30C, still INCLUDE_ASM) enqueues by
 * storing its own second argument (buffer, a single `sw` at 0x0021DFB4) at
 * entry+0x00 and strcpy'ing its own first argument (name, the lbu/sb loop at
 * 0x0021DF90..0x0021DFA8) into entry+0x10; queue_next below hands those two
 * fields back to it in that same order, matching xglCdReadFile's
 * (name, buffer, ...) shape in shared.h.
 */
typedef struct CdRequestPartial {
    void *buffer;
    int offset;
    int length;
    CdCompletionCallback *completion_callback;
    char name[0x70];
} CdRequestPartial;

/*
 * Partial view of LW, limited to fields evidenced by this TU.
 * unmodeled_XX spans are opaque bytes between evidenced fields; naming them
 * `unmodeled_<offset>[<size>]` follows the convention used across the
 * project (e.g. src/main/game.c, src/main/ssd_init.h) instead of inventing
 * members to complete the struct's size.
 */
typedef struct CdReadControl {
    int active_file_descriptor;                 /* +0x00 */
    u8 *write_cursor;                            /* +0x04 */
    int remaining_bytes;                         /* +0x08 */
    int read_poll_mode;                          /* +0x0c */
    CdCompletionCallback *completion_callback;   /* +0x10 */
    u8 unmodeled_014[0xc];                       /* +0x14..0x1f */
    u8 *extract_source;                          /* +0x20 */
    u8 *extract_destination;                     /* +0x24 */
    int extract_byte_count;                      /* +0x28 */
    u8 unmodeled_02c[0x4];                       /* +0x2c..0x2f */
    signed char dispatch_state;                  /* +0x30 */
    u8 unmodeled_031[0x3];                       /* +0x31..0x33 */
    u8 power_off_pending;                        /* +0x34 */
    u8 unmodeled_035[0x9];                       /* +0x35..0x3d */
    u8 queue_cursor;                             /* +0x3e */
    u8 queue_tail;                               /* +0x3f */
    CdRequestPartial requests[32];               /* +0x40..0x103f */
} CdReadControl;

extern int xglCdReadFilePart(const char *name, void *buffer, int mode,
                             CdCompletionCallback *callback, int offset, int length);
extern int sceDevctl(const char *device, int command, const void *input,
                     unsigned int input_size, void *output, unsigned int output_size);
extern int sceCdPause(void);
extern int sceCdPowerOff(u32 *result);
extern u32 sceCdGetReadPos(void);

/*
 * The descriptor word at LW+0 (hdd_error) is read through the shared
 * unsigned byte view of LW: that incomplete type keeps LW absolutely
 * addressed under this object's -G8, as in the original.
 */
static void hdd_error(void)
{
    int *descriptor = (int *)LW;

    sceClose(*descriptor);
    *descriptor = -1;
    xglHddErrorScreen();
}

void xglCdPowerOffCB(void)
{
    LW[0x34] = 1;
}

extern void xglArxExtract(void *destination, void *source);

static int extract(void *destination, void *source, int byte_count)
{
    if (((int *)source)[0] == 0x585241 && ((int *)source)[3] == 0) {
        xglArxExtract(destination, source);
    } else if (destination != source) {
        for (; byte_count > 0; byte_count -= 4) {
            *((int *)destination)++ = *((int *)source)++;
        }
    }
}

static int queue_next(void)
{
    CdRequestPartial *request;

    CD_READ_CONTROL->completion_callback(4,
                                         (CD_READ_CONTROL->queue_tail
                                          - CD_READ_CONTROL->queue_cursor) & 0x1f);
    CD_READ_CONTROL->dispatch_state = 0;
    if (CD_READ_CONTROL->queue_tail != CD_READ_CONTROL->queue_cursor) {
        request = &CD_READ_CONTROL->requests[CD_READ_CONTROL->queue_cursor];
        xglCdReadFilePart(request->name, request->buffer, 1,
                          request->completion_callback, request->offset,
                          request->length);
        CD_READ_CONTROL->queue_cursor = (CD_READ_CONTROL->queue_cursor + 1) & 0x1f;
        return 0;
    }
    return 1;
}

void xglCdControlThread(void)
{
    u32 power_status;

    for (;;) {
        int requested_bytes;
        int transferred;

        xglSleep();
        if (ReadClockInterval > 0)
            ReadClockInterval--;

        if (CD_READ_CONTROL->power_off_pending != 0) {
            sceDevctl(D_004DC2D0, 0x5003, 0, 0, 0, 0);
            sceDevctl(D_004DC2D8, 0x5003, 0, 0, 0, 0);
            sceDevctl(D_004DC2E0, 0x4806, 0, 0, 0, 0);
            sceCdPowerOff(&power_status);
        }

        switch (CD_READ_CONTROL->dispatch_state) {
        case 2:
            switch (CD_READ_CONTROL->read_poll_mode) {
            case 0:
                CD_READ_CONTROL->completion_callback(3, sceCdGetReadPos());
                if (sceCdSync(1) != 0)
                    continue;
                extract(CD_READ_CONTROL->extract_destination,
                        CD_READ_CONTROL->extract_source,
                        CD_READ_CONTROL->extract_byte_count);
                if (queue_next() == 0)
                    continue;
                sceCdPause();
                continue;
            case 1:
            case 2:
                requested_bytes = CD_READ_CONTROL->remaining_bytes;
                if (requested_bytes > 0x10000)
                    requested_bytes = 0x10000;
                transferred = sceRead(CD_READ_CONTROL->active_file_descriptor,
                                      CD_READ_CONTROL->write_cursor,
                                      requested_bytes);
                if (transferred < 0) {
                    hdd_error();
                    CD_READ_CONTROL->queue_cursor =
                        (CD_READ_CONTROL->queue_cursor + 31) & 31;
                    continue;
                }

                CD_READ_CONTROL->write_cursor += transferred;
                CD_READ_CONTROL->remaining_bytes -= transferred;
                CD_READ_CONTROL->completion_callback(
                    3, sceLseek(CD_READ_CONTROL->active_file_descriptor, 0, 1));
                if (CD_READ_CONTROL->remaining_bytes > 0)
                    continue;

                {
                    int partial_sector = transferred & 0x7ff;
                    if (partial_sector != 0) {
                        int remaining_padding = (-partial_sector) & 0x7ff;
                        while (remaining_padding > 0) {
                            *CD_READ_CONTROL->write_cursor++ =
                                (unsigned char)remaining_padding;
                            remaining_padding--;
                        }
                    }
                }
                sceClose(CD_READ_CONTROL->active_file_descriptor);
                CD_READ_CONTROL->active_file_descriptor = -1;
                extract(CD_READ_CONTROL->extract_destination,
                        CD_READ_CONTROL->extract_source,
                        CD_READ_CONTROL->extract_byte_count);
                queue_next();
                break;
            default:
                continue;
            }
            break;
        case 3: {
            CdStreamParam **stream_cursor;
            int stream_index;
            for (stream_cursor = &StrList, stream_index = 0;
                 stream_index <= STR_LIST_LAST_INDEX;
                 stream_index++, stream_cursor++) {
                if (*stream_cursor != 0)
                    xglCdStreamReadRingCore(*stream_cursor);
            }
            break;
        }
        case 0:
        case 1:
            break;
        default:
            CD_READ_CONTROL->dispatch_state = 0;
        }
    }
}

static int BCD2INT(unsigned char x)
{
    return (x >> 4) * 10 + (x & 15);
}

void xglClockRead(XglClock *clock)
{
    if (ReadClockInterval == 0) {
        ReadClockInterval = 10;
        sceCdReadClock(PresentTime);
    }

    clock->year = BCD2INT(PresentTime[7]) + 2000;
    clock->month = BCD2INT(PresentTime[6]);
    clock->day = BCD2INT(PresentTime[5]);
    clock->hour = BCD2INT(PresentTime[3]);
    clock->minute = BCD2INT(PresentTime[2]);
    clock->second = BCD2INT(PresentTime[1]);
    clock->status = 0;
}

/*
 * Converts a calendar record to the number of seconds elapsed since
 * 2000-01-01 00:00:00.  monthday[12..23] holds the cumulative day offset of
 * each month; the leap day of the current year counts from March onwards,
 * and ((year - 1997) >> 2) counts the leap days of the whole years before it.
 */
unsigned int xglClockDayTime2UInt(XglClock *clock_time)
{
    unsigned int seconds;
    unsigned int year;
    unsigned char month;

    month = clock_time->month;
    seconds = (monthday[month + 11] + clock_time->day - 1) * 86400
        + clock_time->hour * 3600 + clock_time->minute * 60
        + clock_time->second;
    year = clock_time->year;
    if (((year - 2000) & 3) == 0 && month >= 3) {
        seconds += 86400;
    }
    return seconds
        + ((year - 2000) * 31536000 + ((year - 1997) >> 2) * 86400);
}

void xglClockUInt2DayTime(XglClock *clock_time, unsigned int elapsed)
{
    XglClock *clock;
    unsigned int year_length = 366;
    unsigned int year = 0;
    unsigned int common_year_length = 365;
    unsigned int leap_year_length = 366;
    unsigned int days;
    unsigned int remainder;
    unsigned int month;
    unsigned int month_length;

    clock = clock_time;
    days = elapsed / 86400;
    remainder = elapsed % 86400;
    clock->hour = remainder / 3600;
    remainder -= clock->hour * 3600;
    clock->minute = remainder / 60;
    remainder -= clock->minute * 60;
    clock->second = remainder;

    while (days >= year_length) {
        year += 1;
        days -= year_length;
        year_length = (year & 3) == 0 ? leap_year_length : common_year_length;
    }
    clock->year = year + 2000;

    for (month = 0; ; month += 1) {
        month_length = monthday[month];
        if (month == 1 && (clock->year & 3) == 0)
            month_length += 1;
        if (days < month_length)
            break;
        days -= month_length;
    }
    clock->month = month + 1;
    clock->day = days + 1;
}

extern int sceCdStatus(void);
extern int sceCdDiskReady(int mode);
extern int sceCdGetDiskType(void);
extern const char D_004D2468[];
extern const char D_004D2478[];
extern const char D_004D2488[];

int xglCdDiskCheck(void)
{
    unsigned char state;
    int type;

    if (LW[0x33] != 0) {
        LW[0x1C] = 1;
        LW[0x1D] = 0x7F;
    }
    state = LW[0x1C];
    if (state != 1) {
        LW[0x1C] = 0;
        if (sceCdStatus() == 1)
            return -2;
        if (sceCdDiskReady(1) != 2)
            return -3;
        type = sceCdGetDiskType();
        if (type == 1)
            return -3;
        if (type == 0)
            return type;
        if (type != 0x14)
            return -1;
        LW[0x1D] = 0;
        if (xglCdGetFileSize(D_004D2468) > 0)
            LW[0x1D] |= 1;
        if (xglCdGetFileSize(D_004D2478) > 0)
            LW[0x1D] |= 2;
        if (xglCdGetFileSize(D_004D2488) > 0)
            LW[0x1D] |= 4;
        LW[0x1C] = 1;
    } else if (sceCdStatus() == state) {
        LW[0x1C] = 0;
        return -2;
    }
    return LW[0x1D];
}

static void xglCdDefaultCallback(int event, int value)
{
    (void)event;
    (void)value;
}

static void xglCdDummyCallback(int event, int value)
{
    (void)event;
    (void)value;
}

extern CdCompletionCallback *callback;

void xglCdReset(void)
{
    CdReadControl *control = CD_READ_CONTROL;

    callback = xglCdDefaultCallback;
    control->queue_cursor = 0;
    control->queue_tail = 0;
    control->dispatch_state = 0;
    StrList = 0;
    sceCdPause();
    sceCdSync(0);
}

CdCompletionCallback *xglCdSetCallback(int callback_address)
{
    CdCompletionCallback *old_callback = callback;

    switch (callback_address) {
    case 0:
        callback = xglCdDefaultCallback;
        break;
    case 1:
        callback = xglCdDummyCallback;
        break;
    case -1:
        break;
    default:
        callback = (CdCompletionCallback *)callback_address;
        break;
    }
    return old_callback;
}

extern unsigned char root_0[6];
typedef struct CdPathPos {
    unsigned char unmodeled_00;
    unsigned char unmodeled_01;
    unsigned char flag;
    unsigned char unmodeled_03;
    int data;
    int unmodeled_08;
    int lbn;
    int size;
    int alt;
} CdPathPos;

static int xglCdGetFilePosSub(CdPathPos *fp, signed char *arg1)
{
    char path[0x100];
    char *ptrs[0x10];
    char *root;
    char *pathp;
    unsigned char *src;
    unsigned char *dst;
    unsigned char b;
    int data;
    int i;

    root = (char *)root_0;
    pathp = path;
    *pathp = *root;
    src = (unsigned char *)fp->data + 1;
    if (((unsigned int)*pathp << 24) != 0) {
        do {
            root++;
            pathp++;
            *pathp = *root;
        } while (((unsigned int)*pathp << 24) != 0);
    }

    ptrs[0] = pathp;
    i = 0;
    b = *src;
    if (b != 0) {
        do {
            int mode = b & 0x40;
            data = b & 0x3f;
            if ((b & 0x80) != 0) {
                int dist = src[1];
                src += 2;
                data = (data + 0xfe) & 0xff;
                i -= dist;
                dst = (unsigned char *)ptrs[i];
                if (data != 0) {
                    do {
                        *dst = *src;
                        src++;
                        data = (data + 0xff) & 0xff;
                        dst++;
                    } while (data != 0);
                }
                i++;
                dst[0] = 0x5c;
                dst[1] = 0;
                ptrs[i] = (char *)(dst + 1);
            } else {
                data = (data + 0xff) & 0xff;
                dst = (unsigned char *)ptrs[i];
                src++;
                if (data != 0) {
                    do {
                        *dst = *src;
                        src++;
                        data = (data + 0xff) & 0xff;
                        dst++;
                    } while (data != 0);
                }
                *dst = 0;

                {
                    int matched = 0;
                    signed char *pa;
                    signed char ca;
                    signed char cp;
                    pathp = path;
                    pa = arg1;
                    ca = *pa;
                    cp = *pathp;
                    for (;;) {
                        if (ca != cp) {
                            break;
                        }
                        if (ca == 0) {
                            matched = 1;
                            break;
                        }
                        pa++;
                        pathp++;
                        ca = *pa;
                        cp = *pathp;
                    }

                    if (matched != 0) {
                        int t;
                        fp->lbn = (src[2] << 16) + (src[1] << 8) + src[0];
                        t = (src[6] << 24) + (src[3] + ((src[4] << 8) + (src[5] << 16)));
                        fp->flag = mode;
                        fp->size = t;
                        if (mode != 0) {
                            fp->alt = (src[9] << 16) + (src[8] << 8) + src[7];
                        } else {
                            fp->alt = t;
                        }
                        return 1;
                    }
                }
                src += 7;
                if (mode != 0) {
                    src += 3;
                }
            }
            b = *src;
        } while (b != 0);
    }

    return 0;
}

typedef struct CdPosView {
    int lbn;
    int stored_bytes;
    unsigned char unmodeled_08[0x1c];
    unsigned char source;
    unsigned char unmodeled_25;
    unsigned char packed;
    unsigned char unmodeled_27;
    int descriptor;
    int size_bytes;
} CdPosView;
typedef struct CdArcView {
    unsigned char state;
    unsigned char unmodeled_01;
    unsigned char packed;
    unsigned char unmodeled_03[5];
    int lbn;
    int file_lbn_offset;
    int stored_bytes;
    int size_bytes;
} CdArcView;
extern CdArchiveEntry D_0093DC58;
extern char arcname_1[12];
extern char arcname_2[12];
extern char arcname_3[12];
extern char arcname_4[12];
extern unsigned char vcd_head_5[0x18];
extern unsigned char hdd_head_6[7];
typedef struct SearchLayerBytes {
    unsigned char byte;
    unsigned char unmodeled_01[0x10];
} SearchLayerBytes;
extern SearchLayerBytes D_0093CC32;
extern int sceCdSync(int mode);
extern int sceCdLayerSearchFile(CdFilePosition *position, signed char *name, unsigned char layer);
extern int sceOpen(const char *path, int flags, ...);
extern int xglHddCheck(void);

static int xglCdGetFilePos(CdFilePosition *file_position, const char *name,
                                       void (*callback)(int event, int value))
{
    char buffer[256];
    const char *path = name;
    CdArcView *entry;
    const char *cursor;
    char *destination;
    const char *absolute_source;
    char *absolute_destination;
    char *scan;
    int descriptor;
    long offset;
    unsigned char *header;
    int size;
    int source;
    CdPosView *position = (CdPosView *)file_position;

    /* p4 */
    position->source = 0;
    if (path[0] != 92) {
        for (scan = (char *)path; *scan != 0; scan++) {
            if (*scan == 47)
                *scan = 92;
        }
    }
    for (;;) {
        entry = (CdArcView *)&ArcHeader[0];
        if (entry->state != 0xff && xglCdGetFilePosSub((CdPathPos *)entry, (signed char *)name) != 0) {
            name = arcname_1;
            position->source = entry->state;
            break;
        }
        entry = (CdArcView *)&D_0093DC58;
        if (entry->state != 0xff && xglCdGetFilePosSub((CdPathPos *)entry, (signed char *)name) != 0) {
            name = arcname_2;
            position->source = entry->state;
            goto resolved;
        }
        entry++;
        if (entry->state != 0xff && xglCdGetFilePosSub((CdPathPos *)entry, (signed char *)name) != 0) {
            name = arcname_3;
            position->source = entry->state;
            goto resolved;
        }
        entry++;
        if (entry->state != 0xff && xglCdGetFilePosSub((CdPathPos *)entry, (signed char *)name) != 0) {
            name = arcname_4;
            position->source = entry->state;
            goto resolved;
        }
        entry = 0;
        goto resolved;
    }
resolved:
    if (name[0] == 92) {
        absolute_source = name + 1;
        absolute_destination = buffer;
        while ((*absolute_destination = *absolute_source) != 0) {
            absolute_source++;
            absolute_destination++;
        }
        position->source = 1;
    } else {
        destination = buffer;
        source = position->source;
        switch (source) {
        case 0:
            buffer[0] = 92;
            destination = buffer + 1;
            for (cursor = name; *cursor != 0; cursor++, destination++) {
                if ((char)*cursor < 97)
                    *destination = *cursor;
                else
                    *destination = *cursor - 32;
            }
            destination[0] = 59;
            destination[1] = 49;
            destination[2] = 0;
            break;
        case 1:
        case 2: {
            if (position->source == 1)
                cursor = (const char *)vcd_head_5;
            else
                cursor = (const char *)hdd_head_6;
            while ((*destination = *cursor) != 0) {
                cursor++;
                destination++;
            }
            cursor = name;
            for (;;) {
                if (*cursor == 92)
                    *destination = 47;
                else
                    *destination = *cursor;
                if (*cursor == 0)
                    break;
                cursor++;
                destination++;
            }
            break;
        }
        }
    }
    callback(0, (int)path);
    switch (position->source) {
    case 0:
        if (entry != 0) {
            position->lbn = entry->lbn + entry->file_lbn_offset;
            position->stored_bytes = entry->stored_bytes;
            position->packed = entry->packed;
            position->size_bytes = entry->size_bytes;
        } else {
            sceCdSync(0);
            sceCdDiskReady(0);
            if (sceCdLayerSearchFile((CdFilePosition *)position, (signed char *)buffer, D_0093CC32.byte) == 0)
                return 0;
            position->packed = 1;
            position->size_bytes = position->stored_bytes;
        }
        break;
    case 2:
        if (xglHddCheck() < 0)
            return xglCdGetFilePos(file_position, path, callback);
    case 1:
        descriptor = sceOpen(buffer, 1);
        if (descriptor < 0)
            return 0;
        if (entry != 0) {
            position->lbn = entry->lbn + entry->file_lbn_offset;
            position->stored_bytes = entry->stored_bytes;
            position->packed = entry->packed;
            position->size_bytes = entry->size_bytes;
        } else {
            position->lbn = 0;
            header = (unsigned char *)(((unsigned int)WorkEnd + 63) & -64);
            sceRead(descriptor, header, 16);
            if (*(int *)header == 0x585241 && *(int *)(header + 12) == 0) {
                position->packed = 1;
                position->size_bytes = *(int *)(header + 4);
            } else {
                position->packed = 0;
            }
            size = sceLseek(descriptor, 0, 2);
            position->stored_bytes = size;
            if (position->packed == 0)
                position->size_bytes = size;
        }
        offset = (long)(unsigned int)position->lbn << 11;
        sceLseek(descriptor, 0, 0);
        while (offset > 0x40000000) {
            sceLseek(descriptor, 0x40000000, 1);
            offset -= 0x40000000;
        }
        sceLseek(descriptor, (int)offset, 1);
        position->descriptor = descriptor;
        break;
    }
    return (int)file_position;
}

int xglCdGetFileData(const char *path, CdFilePosition *file_position)
{
    return (xglCdGetFilePos(file_position, path, xglCdDummyCallback) != 0) ? 0 : -1;
}

int xglCdGetFileSize(const char *path)
{
    CdFilePosition pos;
    int mode;

    if (xglCdGetFilePos(&pos, path, xglCdDummyCallback) == 0) {
        return -1;
    }
    if (pos.unmodeled_04[0x20] != 0) {
        mode = pos.unmodeled_04[0x20];
        if (mode >= 0) {
            if (mode < 3) {
                sceClose(*(int *)&pos.unmodeled_04[0x24]);
            }
        }
    }
    return *(int *)&pos.unmodeled_04[0x28];
}

typedef struct RfpPos {
    int lbn;
    int stored_bytes;
    unsigned char unmodeled_08[0x1c];
    unsigned char source;
    unsigned char unmodeled_25;
    unsigned char packed;
    unsigned char unmodeled_27;
    int descriptor;
    int size_bytes;
} RfpPos;
extern int sceCdGetError(void);

int xglCdReadFilePart(const char *name, void *buffer, int mode,
                      CdCompletionCallback *notify, int offset, int length)
{
    RfpPos position;
    CdReadMode read_mode;
    CdRequestPartial *request;
    const char *source;
    char *destination;
    int asynchronous;
    int result;
    int padding;
    int partial;

    switch ((int)notify) {
    case 0:
        notify = callback;
        break;
    case 1:
        notify = xglCdDummyCallback;
        break;
    case 2:
        notify = xglCdDefaultCallback;
        break;
    }
    asynchronous = mode & 1;
    if (asynchronous != 0) {
        if (CD_READ_CONTROL->dispatch_state != 0) {
            if (CD_READ_CONTROL->dispatch_state != 3) {
                request = &CD_READ_CONTROL->requests[CD_READ_CONTROL->queue_tail];
                source = name;
                destination = request->name;
                while ((*destination++ = *source++) != 0)
                    ;
                request->buffer = buffer;
                request->offset = offset;
                request->length = length;
                request->completion_callback = notify;
                CD_READ_CONTROL->queue_tail = (CD_READ_CONTROL->queue_tail + 1) & 0x1f;
                return 0;
            }
        }
    }
    if (CD_READ_CONTROL->dispatch_state != 0)
        return -3;
    if (xglCdGetFilePos((CdFilePosition *)&position, name, notify) == 0)
        return -1;
    position.lbn += offset >> 11;
    if (length > 0)
        position.stored_bytes = length;
    notify(1, (position.stored_bytes + 2047) & -2048);
    CD_READ_CONTROL->completion_callback = notify;
    CD_READ_CONTROL->extract_destination = buffer;
    CD_READ_CONTROL->extract_byte_count = (position.stored_bytes + 2047) & -2048;
    if (position.packed == 0 || (mode & 2) != 0)
        CD_READ_CONTROL->extract_source = buffer;
    else
        CD_READ_CONTROL->extract_source = (u8 *)buffer + ((position.size_bytes + 2047) & -2048)
            - CD_READ_CONTROL->extract_byte_count;
    CD_READ_CONTROL->read_poll_mode = position.source;
    switch (position.source) {
    case 0:
    retry_cd:
        sceCdSync(0);
        sceCdDiskReady(0);
        read_mode.try_count = 0;
        read_mode.spindle_control = CD_READ_CONTROL->unmodeled_031[0];
        read_mode.data_pattern = 0;
        sceCdRead(position.lbn, CD_READ_CONTROL->extract_byte_count / 2048, CD_READ_CONTROL->extract_source,
                  &read_mode);
        if (asynchronous != 0) {
            CD_READ_CONTROL->dispatch_state = 2;
            return position.size_bytes;
        }
        CD_READ_CONTROL->dispatch_state = 1;
        while (sceCdSync(1) > 0)
            notify(2, sceCdGetReadPos());
        result = sceCdGetError();
        if ((unsigned int)result >= 2) {
            notify(-2, result);
            goto retry_cd;
        }
        break;
    case 1:
    case 2:
        CD_READ_CONTROL->active_file_descriptor = position.descriptor;
        CD_READ_CONTROL->write_cursor = CD_READ_CONTROL->extract_source;
        CD_READ_CONTROL->remaining_bytes = position.stored_bytes;
        if (asynchronous != 0) {
            CD_READ_CONTROL->dispatch_state = 2;
            return position.size_bytes;
        }
        CD_READ_CONTROL->dispatch_state = 1;
        do {
            if (CD_READ_CONTROL->remaining_bytes > 0xffff)
                result = sceRead(CD_READ_CONTROL->active_file_descriptor,
                                 CD_READ_CONTROL->write_cursor, 0x10000);
            else
                result = sceRead(CD_READ_CONTROL->active_file_descriptor,
                                 CD_READ_CONTROL->write_cursor,
                                 CD_READ_CONTROL->remaining_bytes);
            if (result < 0) {
                hdd_error();
                CD_READ_CONTROL->dispatch_state = 0;
                return xglCdReadFilePart(name, buffer, mode, notify, offset, length);
            }
            CD_READ_CONTROL->write_cursor += result;
            CD_READ_CONTROL->remaining_bytes -= result;
            notify(2, position.stored_bytes - CD_READ_CONTROL->remaining_bytes);
        } while (CD_READ_CONTROL->remaining_bytes > 0);
        partial = result & 0x7ff;
        if (partial != 0) {
            padding = -partial & 0x7ff;
            while (padding > 0) {
                *CD_READ_CONTROL->write_cursor++ = padding;
                padding--;
            }
        }
        sceClose(CD_READ_CONTROL->active_file_descriptor);
        CD_READ_CONTROL->active_file_descriptor = -1;
        break;
    }
    if ((mode & 2) == 0)
        extract(CD_READ_CONTROL->extract_destination, CD_READ_CONTROL->extract_source,
                CD_READ_CONTROL->extract_byte_count);
    notify(4, 0);
    CD_READ_CONTROL->dispatch_state = 0;
    return (position.size_bytes + 2047) & -2048;
}

int xglCdReadFile(const char *name, void *buffer, int mode, int flags)
{
    return xglCdReadFilePart(name, buffer, mode, (CdCompletionCallback *)flags, 0, -1);
}

extern int sceCdBreak(void);

void xglCdReadCancel(void)
{
    int *fd;

    CD_READ_CONTROL->queue_tail = CD_READ_CONTROL->queue_cursor;
    if (CD_READ_CONTROL->dispatch_state == 1) {
        if (CD_READ_CONTROL->read_poll_mode != 0) {
            if (CD_READ_CONTROL->read_poll_mode >= 0) {
                if (CD_READ_CONTROL->read_poll_mode < 3)
                    CD_READ_CONTROL->remaining_bytes = 0;
            }
        } else {
            goto brk;
        }
    } else if (CD_READ_CONTROL->dispatch_state == 2) {
        if (CD_READ_CONTROL->read_poll_mode != 0) {
            if (CD_READ_CONTROL->read_poll_mode >= 0) {
                if (CD_READ_CONTROL->read_poll_mode < 3)
                    goto cls;
            }
        } else {
brk:
            sceCdBreak();
            sceCdPause();
        }
    }
    goto out;
cls:
    fd = (int *)LW;
    if (*fd >= 0) {
        sceClose(*fd);
        *fd = -1;
    }
out:
    CD_READ_CONTROL->dispatch_state = 0;
}

int xglCdSync(void)
{
    /*
     * Byte +0x30 is read signed (lb at 0x0021e404; config/header-canon.json
     * LW byte_required io-main-0021e400); the shared LW is the unsigned view.
     */
    return (signed char)LW[0x30] != 0;
}

typedef struct CdFilePositionView {
    int lbn;
    int stored_bytes;
    unsigned char unmodeled_08[0x1c];
    unsigned char source;
    unsigned char unmodeled_25[3];
    int descriptor;
} CdFilePositionView;
extern int sceCdStInit(int bufmax, int nbuf, int buffer);
extern int sceCdStStart(int lbn, CdReadMode *read_mode);
extern int sceSifAllocIopHeap(int size);
extern int sceCdSync(int mode);

int xglCdStreamOpen(CdStreamParam *stream, const char *path)
{
    CdFilePositionView position;
    CdReadMode read_mode;
    CdStreamParam **slot;
    int index;
    int mode;

    /* s1 */
    CD_READ_CONTROL->dispatch_state = 3;
    if (xglCdGetFilePos(&position, path, xglCdDummyCallback) == 0) {
        CD_READ_CONTROL->dispatch_state = 0;
        return -1;
    }
    stream->mode = position.source;
    stream->file_byte_count = position.stored_bytes;
    mode = position.source;
    switch (mode) {
    case 0:
        sceCdSync(0);
        sceCdDiskReady(0);
        stream->descriptor = position.lbn;
        stream->iop_buffer = sceSifAllocIopHeap(((short)stream->sector_count << 11) + 128);
        sceCdStInit((short)stream->sector_count, (short)stream->block_count,
                    (stream->iop_buffer + 127) & -128);
        read_mode.try_count = 0;
        read_mode.spindle_control = 0;
        read_mode.data_pattern = 0;
        sceCdStStart(stream->descriptor, &read_mode);
        break;
    case 1:
    case 2:
        stream->descriptor = position.descriptor;
        break;
    }
    stream->remaining = stream->file_byte_count;
    stream->ring_size = 2048;
    stream->state = 0;
    if (stream->ring.buffer != 0) {
        stream->ring.read_position = 0;
        stream->ring.write_position = 0;
        for (slot = &StrList, index = 0; index <= STR_LIST_LAST_INDEX; index++, slot++) {
            if (*slot == 0) {
                *slot = stream;
                break;
            }
        }
    }
    return 0;
}

int xglCdStreamRead(CdStreamParam *stream, unsigned char *buffer, int bytes)
{
    int bytes_read;
    int remaining;
    int pad;

    /* bytes is a byte count; StreamReadRingCoreSub takes a sector count
     * (sll ...,0xb at the call site), matching CD_SECTOR_BYTE_SHIFT below. */
    bytes_read = StreamReadRingCoreSub(stream, buffer, bytes >> 11);
    remaining = stream->remaining - bytes_read;
    if (remaining <= 0) {
        remaining = 0;
        stream->state = 1;
    }
    /* ring_size is a power of two; pad is the read's own tail within the
     * ring, zero-filled below and folded into the returned byte count. */
    pad = bytes_read & (stream->ring_size - 1);
    stream->remaining = remaining;
    if (pad > 0) {
        buffer += bytes_read;
        bytes_read += pad;
        do {
            pad--;
            *buffer++ = 0;
        } while (pad > 0);
    }
    return bytes_read;
}

static int StreamReadRingCoreSub(CdStreamParam *stream, void *buffer, int sectors)
{
    /*
     * CD sectors are CD_SECTOR_BYTES bytes; CD_SECTOR_BYTE_SHIFT converts a
     * sector count to a byte count with a left shift, matching the
     * original sll ...,0xb.
     */
    enum { CD_SECTOR_BYTE_SHIFT = 11 };
    int read_error;
    int mode;
    int result = 0;

    mode = stream->mode;
    switch (mode) {
    case 0:
        result = sceCdStRead(sectors, buffer, 1, &read_error);
        result <<= CD_SECTOR_BYTE_SHIFT;
        break;
    case 1:
    case 2:
        result = sceRead(stream->descriptor, buffer, sectors << CD_SECTOR_BYTE_SHIFT);
        break;
    }
    return result;
}

static void StreamReadRingCoreNormal(CdStreamParam *stream)
{
    /* Sectors to read: [0] up to the first boundary, [1] after the wrap. */
    int sectors[2];
    int available;
    int mode;
    int bytes;
    int end_offset;
    int padding;
    int remaining;

    u8 *cursor;
    u8 *end;

    if (stream->remaining > 0) {
        if (stream->ring.write_position <= stream->ring.read_position) {
            sectors[1] = 0;
            sectors[0] = (stream->ring.read_position - stream->ring.write_position) / 2048;
        } else {
            sectors[0] = (stream->ring.capacity - stream->ring.write_position) / 2048;
            sectors[1] = stream->ring.read_position / 2048;
        }
        available = 0;
        mode = stream->mode;
        switch (mode) {
        case 0:
            available = sceCdStStat();
            break;
        case 1:
        case 2:
            available = 32;
            break;
        }
        if (available < sectors[0]) {
            sectors[0] = available;
            sectors[1] = 0;
        } else if (available - sectors[0] < sectors[1]) {
            sectors[1] = available - sectors[0];
        }
        if (sectors[0] > 0) {
            cursor = stream->ring.buffer + stream->ring.write_position;
            bytes = StreamReadRingCoreSub(stream, cursor, sectors[0]);
            if (sectors[1] > 0) {
                cursor = stream->ring.buffer;
                bytes = StreamReadRingCoreSub(stream, cursor, sectors[1]);
            }
            remaining = stream->remaining - ((sectors[0] + sectors[1]) << 11);
            if (remaining <= 0) {
                end = cursor + bytes;
                stream->state = 1;
                end_offset = (end - stream->ring.buffer) & (stream->ring_size - 1);
                remaining = 0;
                if (end_offset > 0) {
                    padding = stream->ring_size - end_offset;
                    cursor = end;
                    if (padding > 0) {
                        do {
                            padding--;
                            *cursor++ = 0;
                        } while (padding > 0);
                    }
                }
            }
            stream->remaining = remaining;
            stream->ring.write_position =
                (stream->ring.write_position + ((sectors[0] + sectors[1]) << 11))
                % stream->ring.capacity;
        }
    }
}

static void StreamReadRingCoreXss(CdStreamParam *stream)
{
    StreamXssBuffer *xss;
    StreamRing *ring;
    int available;
    int mode;

    if (stream->remaining > 0) {
        available = 0;
        mode = stream->mode;
        xss = stream->xss;
        switch (mode) {
        case 0:
            available = sceCdStStat();
            break;
        case 1:
        case 2:
            available = 32;
            break;
        }
        while (available > 0 && stream->remaining > 0) {
            if (xss->sectors <= 0) {
                StreamReadRingCoreSub(stream, xss->source, 1);
                xss->position = xss->source;
                xss->value = 0;
                xss->mode = 0;
                xss->sectors = 0x4000;
            } else {
                if (xss->mode == 0) {
                    xss->value = *xss->position;
                    xss->position += 1;
                    xss->mode = 8;
                }
                ring = ((xss->value >> (8 - xss->mode)) & 1) == 0
                    ? &stream->ring : &stream->alternate_ring;
                if (ring->write_position == ring->read_position)
                    break;
                xss->mode--;
                xss->sectors--;
                StreamReadRingCoreSub(stream, ring->buffer + ring->write_position, 1);
                ring->write_position = (ring->write_position + 2048) % ring->capacity;
            }
            available--;
            stream->remaining -= 2048;
        }
        if (stream->remaining <= 0) {
            stream->remaining = 0;
            stream->state = 1;
        }
    }
}

void xglCdStreamReadRingCore(CdStreamParam *stream)
{
    if (stream->xss)
        StreamReadRingCoreXss(stream);
    else
        StreamReadRingCoreNormal(stream);
}

int xglCdStreamReadRing(CdStreamParam *stream, int bytes)
{
    int aligned;

    aligned = (bytes + 2047) & -2048;
    stream->ring.read_position =
        (stream->ring.read_position + aligned) % stream->ring.capacity;
    do {
        xglCdStreamReadRingCore(stream);
    } while (stream->remaining != 0 &&
             stream->ring.write_position != stream->ring.read_position);
    return 0;
}

int xglCdStreamRewind(CdStreamParam *stream)
{
    int mode;

    stream->state = 0;
    stream->remaining = stream->file_byte_count;
    mode = stream->mode;
    switch (mode) {
    case 0:                                 /* CD stream */
        sceCdStSeek(stream->descriptor);
        break;
    case 1:                                 /* host / memory-card file */
    case 2:
        sceLseek(stream->descriptor, 0, 0);
        break;
    }
    return 0;
}

int xglCdStreamClose(CdStreamParam *stream)
{
    CdStreamParam **slot;
    int index;
    int mode;

    slot = &StrList;
    for (index = 0; index < 1; ++index, ++slot) {
        if (*slot == stream) {
            *slot = 0;
            break;
        }
    }

    if (stream->descriptor == -1)
        return 0;

    mode = stream->mode;
    switch (mode) {
    case 0:                                 /* CD stream */
        sceCdStStop();
        if (stream->iop_buffer != 0)
            sceSifFreeIopHeap((void *)stream->iop_buffer);
        stream->descriptor = -1;
        break;
    case 1:                                 /* host / memory-card file */
    case 2:
        sceClose(stream->descriptor);
        break;
    }
    LW[0x30] = 0;
    return 0;
}

void xglCdStreamParamInit(CdStreamParam *param)
{
    param->sector_count = 64;
    param->block_count = 16;
    param->descriptor = -1;
    param->iop_buffer = 0;
    param->state = 0;
    param->xss = 0;
    param->ring.buffer = 0;
}

extern char listname_7[];
typedef struct FileSelectList {
    char pad0[0x10];
    char path[0x100];
    unsigned char *buffer;
    unsigned int size;
} FileSelectList;

static void FileSelectListReload(FileSelectList *list)
{
    char name[256];
    unsigned int i;
    unsigned char *q;
    unsigned char *p;
    char *s;
    char *d;
    int fd;

    q = list->buffer;
    for (i = 0; i < list->size; i++)
        *q++ = 0;
    d = name;
    for (s = list->path; *s != 0; s++)
        *d++ = *s;
    for (s = listname_7; *s != 0; s++)
        *d++ = *s;
    *d = 0;
    fd = sceOpen(name, 1);
    sceRead(fd, list->buffer, list->size);
    sceClose(fd);
    for (p = list->buffer; *p != 0; p++) {
        if (*p == '\n')
            *p = 0;
    }
}

typedef struct CdFileSelectU {
    int x;                                         /* +0x000 */
    int y;                                         /* +0x004 */
    int rows;                                      /* +0x008 */
    int state;                                     /* +0x00c */
    char directory[0x100];                         /* +0x010 */
    char *list;                                    /* +0x110 */
    unsigned int list_size;                        /* +0x114 */
    void (*print)(int x, int y, const char *format, ...); /* +0x118 */
    char path[0x100];                              /* +0x11c */
    unsigned short entry_count;                    /* +0x21c */
    short first_row;                               /* +0x21e */
    short cursor;                                  /* +0x220 */
    unsigned char unmodeled_222[2];                /* +0x222 */
    int list_index;                                /* +0x224 */
    char *list_position;                           /* +0x228 */
    unsigned char unmodeled_22c[0x268 - 0x22c];    /* +0x22c */
} CdFileSelectU;

static void FileSelectSub(CdFileSelectU *select)
{
    char prefix[256];
    char *src;
    char *dst;
    char *name;
    char *probe;
    char *part;
    char *cursor;
    char *last;
    char first;
    int prefix_start;
    unsigned char character;
    char expected;
    int mismatch;

    switch (select->state) {
    case 0:
    default:
        FileSelectListReload((FileSelectList *)select);
        select->path[0] = 0;
        select->list_index = 0;
        select->list_position = select->list;
        select->entry_count = 1;
        select->first_row = 0;
        select->state = 1;
    case 1:
        src = select->path;
        dst = prefix;
        character = *src;
        if (*src == 0) {
            prefix[0] = '.';
            prefix[1] = ':';
            prefix[2] = 0;
        } else {
            do {
                *dst++ = character;
                src++;
                character = *src;
            } while (*src != 0);
            *dst = 0;
            dst[-1] = ':';
        }
        name = select->list;
        prefix_start = 0;
        while (*name != 0) {
            first = *name;
            if (first == '.')
                name += 2;
            mismatch = 0;
            part = prefix + prefix_start;
            if (*part != 0) {
                if (*name != 0 && *name == *part) {
                    for (;;) {
                        name++;
                        expected = *++part;
                        if (expected == 0)
                            break;
                        if (*name == 0 || *name != expected) {
                            mismatch = 1;
                            break;
                        }
                    }
                } else {
                    mismatch = 1;
                }
            }
            if (!mismatch) {
                do {
                    select->list_position = name + 1;
                } while (0);
                break;
            }
            {
                unsigned char character;

                while ((character = *name++) != 0) {
                    do {
                        character = *name++;
                    } while (character != 0);
                }
            }
        }
        probe = select->list_position;
        select->entry_count = 0;
        if (select->path[0] != 0)
            select->entry_count = 1;
        if (*probe != 0) {
            unsigned char character;

            probe++;
            for (;;) {
                character = *probe++;
                if (character != 0)
                    continue;
                select->entry_count++;
                character = *probe;
                if (character == 0)
                    break;
            }
        }
        select->state = 2;
        break;
    case 2:
        return;
    case 3:
        last = select->path;
        for (cursor = last; *cursor != 0; cursor++) {
            if (*cursor == '/') {
                if (cursor[1] == 0) {
                    *last = 0;
                    break;
                }
                last = cursor + 1;
            }
        }
        *last = 0;
        select->state = 1;
        break;
    }
}

typedef struct CdFileSelect {
    int x;                                         /* +0x000 */
    int y;                                         /* +0x004 */
    int rows;                                      /* +0x008 */
    int state;                                     /* +0x00c */
    char directory[0x100];                         /* +0x010 */
    char *list;                                    /* +0x110 */
    unsigned int list_size;                        /* +0x114 */
    void (*print)(int x, int y, const char *format, ...); /* +0x118 */
    char path[0x100];                              /* +0x11c */
    unsigned short entry_count;                    /* +0x21c */
    short first_row;                               /* +0x21e */
    short cursor;                                  /* +0x220 */
    unsigned char unmodeled_222[2];                /* +0x222 */
    int unmodeled_224;                             /* +0x224 */
    char *list_position;                           /* +0x228 */
    unsigned char unmodeled_22c[0x268 - 0x22c];    /* +0x22c */
} CdFileSelect;
typedef struct PadDataXgl {
    unsigned char unmodeled_00[0x2a];
    unsigned short pressed;                        /* +0x2a */
    unsigned short repeated;                       /* +0x2c */
} PadDataXgl;
extern PadDataXgl PadData;
extern const char D_004D2498[];
extern const char D_004DC2F8[];
extern const char D_004DC300[];
extern const char D_004DC308[];
extern const char D_004DC310[];

int xglCdFileSelect(CdFileSelect *select)
{
    int index;
    int skipped;
    int end_row;
    int rows;
    int count;
    int x;
    int y;
    unsigned char *list;
    char *last;
    char *path;
    char *tail;
    unsigned char *entry;

    FileSelectSub(select);
    /* c2 */
    if ((PadData.pressed & 0x40) != 0)
        return 2;
    if (PadData.repeated & 0x1000)
        select->cursor--;
    if (select->cursor < 0)
        select->cursor = select->entry_count - 1;
    if (select->cursor - select->first_row <= 0)
        select->first_row = select->cursor - 1;
    if (select->first_row < 0)
        select->first_row = 0;
    if (PadData.repeated & 0x4000)
        select->cursor++;
    if (select->entry_count - 1 < select->cursor)
        select->cursor = 0;
    if (select->cursor - select->first_row > select->rows - 2)
        select->first_row = select->cursor - select->rows + 2;
    if ((unsigned int)(select->entry_count - select->rows) < (unsigned int)select->first_row)
        select->first_row = select->entry_count - select->rows;
    if (PadData.pressed & 0x20) {
        entry = (unsigned char *)select->list_position;
        if (select->path[0] != 0)
            select->cursor--;
        if (select->cursor < 0) {
            path = select->path;
            select->cursor = 0;
            select->state = 3;
            select->first_row = 0;
        } else {
            for (skipped = 0; skipped < select->cursor; skipped++) {
                while (*entry++ != 0)
                    ;
            }
            last = (char *)entry;
            while (last[1] != 0)
                last++;
            path = select->path;
            tail = path;
            while (*tail != 0)
                tail++;
            do {
                *tail++ = *entry;
            } while (*entry++ != 0);
            if (*last == '/') {
                select->cursor = 0;
                select->state = 1;
                select->first_row = 0;
            } else {
                select->cursor = 0;
                select->state = 3;
                select->first_row = 0;
                return 1;
            }
        }
    } else {
        path = select->path;
    }
    x = select->x;
    y = select->y;
    list = (unsigned char *)select->list_position;
    select->print(x, y, D_004D2498, path);
    count = select->entry_count;
    rows = select->rows;
    if (rows < count)
        end_row = rows;
    else
        end_row = count;
    end_row += select->first_row;
    index = select->path[0] != 0;
    y += 12;
    if (select->first_row > 0)
        select->print(x, y, D_004DC2F8);
    for (; index < select->first_row; index++) {
        while (*list++ != 0)
            ;
    }
    for (index = select->first_row; index < end_row; index++) {
        if (index == select->cursor)
            select->print(x + 8, y, D_004DC300);
        if (index == 0 && select->path[0] != 0) {
            select->print(x + 16, y, D_004DC308);
        } else {
            select->print(x + 16, y, list);
            while (*list++ != 0)
                ;
            if (*list == 0)
                return 0;
        }
        y += 8;
    }
    if (*list != 0)
        select->print(x, y - 8, D_004DC310);
    return 0;
}

extern int sceSifLoadModule(const char *filename, int args, const char *argp);
extern char head_8[13];
extern char tail_9[7];

void xglCdSifLoadModule(char *name, char *argp) {
    char buf[256];
    char *s;
    char *d;
    char *p;
    int len;

    d = buf;
    for (s = head_8; (*d = *s) != 0; s++, d++) {
    }
    for (s = name; *s != 0; s++, d++) {
        if (*s < 'a') {
            *d = *s;
        } else {
            *d = *s - 0x20;
        }
    }
    for (s = tail_9; (*d = *s) != 0; s++, d++) {
    }
    len = 0;
    if (argp != 0) {
        p = argp;
        len = 1;
        if (p[0] != 0 || p[1] != 0) {
            do {
                do {
                    p++;
                    len++;
                } while (*p != 0);
            } while (p[1] != 0);
        }
    }
    sceSifLoadModule(buf, len, argp);
}

extern void FlushCache(int);
extern int sceSifLoadElf(char *, void *);
extern unsigned char loaded_overlay;
extern unsigned char head_10[9];
extern unsigned char tail_11[7];

void xglCdLoadOverlay(int arg0)
{
    char tmp[16];
    unsigned char path[0x100];
    unsigned char *src;
    unsigned char *dst;
    unsigned char c;

    if (arg0 != loaded_overlay) {
        loaded_overlay = arg0;
        src = head_10;
        dst = path;
        if (((unsigned int)(*dst = *src) << 24) != 0) {
            do {
                src++;
                dst++;
            } while (((unsigned int)(*dst = *src) << 24) != 0);
        }
        dst[0] = 'O';
        dst[1] = 'V';
        dst[2] = arg0 / 10 + '0';
        dst[3] = arg0 % 10 + '0';
        dst += 4;
        src = tail_11;
        while ((((unsigned int)(*dst = *src)) << 24) != 0) {
            src++;
            dst++;
        }
        FlushCache(0);
        FlushCache(2);
        sceSifLoadElf((char *)path, tmp);
        FlushCache(0);
        FlushCache(2);
    }
}

static void xglCdArcInitSub0(const int *archive_entry, void *destination, int sectors)
{
    int lbn;
    CdReadMode read_mode;

    sceCdSync(0);
    sceCdDiskReady(0);
    if (sectors == 0) {
        lbn = archive_entry[0];
        sectors = 1;
    } else {
        lbn = archive_entry[0] + 1;
    }
    read_mode.spindle_control = 1;
    read_mode.try_count = 0;
    read_mode.data_pattern = 0;
    sceCdRead(lbn, sectors, destination, &read_mode);
    while (sceCdSync(1) > 0)
        ;
}

/* A CD sector is fixed at 2048 bytes on this drive/read path (see also
 * xglCdStreamReadRing's own `& -2048` sector rounding earlier in this TU). */
#define CD_SECTOR_BYTES 2048

static int xglCdArcInitSub1(CdArchiveEntry *archive_entry,
                            const char *path, unsigned char *destination)
{
    CdFilePosition file_position;
    int result;
    int header_count;

    result = xglCdGetFilePos(&file_position, path, xglCdDummyCallback);
    if (result != 0) {
        archive_entry->destination = destination;
        archive_entry->lbn = file_position.lbn;
        archive_entry->state = 0;

        /* xglCdArcInitSub0 only reads its `archive_entry` argument's leading
         * word as an LBN, so the file position's own lbn field (the one
         * evidenced member of CdFilePosition) is exactly what it needs. */
        xglCdArcInitSub0(&file_position.lbn, destination, 0);

        /* Sector 0 of the file is the archive header; its first byte is the
         * header's own sector count. */
        header_count = destination[0];
        if (header_count >= 2) {
            /* The header spans more than one sector: read the remaining
             * header_count - 1 sectors right after the first. */
            xglCdArcInitSub0(&file_position.lbn, destination + CD_SECTOR_BYTES,
                             header_count - 1);
        }
        result = header_count + 1;
    }
    return result;
}

/*
 * Archive table-of-contents walk: archive_data is a header buffer filled by
 * xglCdArcInitSub1/xglCdArcInit, and this finds the byte right after the
 * table's last record so the caller can place fresh data there.
 *
 * Each record begins with a one-byte length/flags field:
 *   - ARC_RECORD_EXTENDED set: no fixed header past this byte.
 *   - ARC_RECORD_EXTENDED clear: a fixed ARC_RECORD_HEADER_BYTES-byte header
 *     follows, extended by ARC_RECORD_TYPE_EXTRA_BYTES more bytes when
 *     ARC_RECORD_TYPE_EXTENDED is also set.
 *   - ARC_RECORD_LENGTH_MASK (the low bits) is the record's payload length,
 *     walked past after the fixed header, if any.
 * A zero length/flags byte ends the table.
 */
#define ARC_RECORD_EXTENDED        0x80
#define ARC_RECORD_TYPE_EXTENDED   0x40
#define ARC_RECORD_LENGTH_MASK     0x3f
#define ARC_RECORD_HEADER_BYTES    7
#define ARC_RECORD_TYPE_EXTRA_BYTES 3
#define ARC_TABLE_ALIGN_BYTES      64

static unsigned char *xglCdArcInitSub2(unsigned char *archive_data)
{
    unsigned char *cursor = archive_data + 1;
    unsigned char value = *cursor;

    while (value != 0) {
        int high_bit = value & ARC_RECORD_EXTENDED;
        int type_flag = value & ARC_RECORD_TYPE_EXTENDED;
        if (high_bit == 0) {
            cursor += ARC_RECORD_HEADER_BYTES;
            if (type_flag != 0)
                cursor += ARC_RECORD_TYPE_EXTRA_BYTES;
        }
        value &= ARC_RECORD_LENGTH_MASK;
        cursor += value;
        value = *cursor;
    }

    /* Round up to the next ARC_TABLE_ALIGN_BYTES-byte boundary and write a
     * two-byte zero terminator marking the end of the table. */
    cursor = (unsigned char *)(((u32)cursor + ARC_TABLE_ALIGN_BYTES)
        & (u32)-ARC_TABLE_ALIGN_BYTES);
    cursor[1] = 0;
    cursor[0] = 0;
    return cursor;
}

extern const char D_004D24A8[];
extern const char D_004D24C0[];
extern const char D_004D2468[];
extern const char D_004D2478[];
extern const char D_004D2488[];
extern CdArchiveEntry D_0093DC58;
extern CdArchiveEntry D_0093DC70;

static void xglCdArcInit(void)
{
    u8 *base = WorkEnd;
    int count = 0;
    int fd;
    CdArchiveEntry *e;
    u8 *p;

    while (xglHddActivate(-1) != 0 && (fd = sceOpen(D_004D24A8, 1)) >= 0) {
        ArcHeader[0] = 2;
        *(u8 **)(ArcHeader + 4) = base;
        *(int *)(ArcHeader + 8) = 0;
        sceRead(fd, base, 0x800);
        count = base[0];
        sceRead(fd, base + 0x800, (count << 11) - 0x800);
        sceClose(fd);
        break;
    }
    if (count == 0) {
        if (xglCdArcInitSub1((CdArchiveEntry *)ArcHeader, D_004D24C0, base) == 0)
            return;
        xglHddActivate(0x100);
    }
    if (LW[0x33] != 0) {
        unsigned char *(*sub2)(unsigned char *) = xglCdArcInitSub2;
        base = sub2(base);
        while (xglCdArcInitSub1(&D_0093DC58, D_004D2468, base) <= 0)
            ;
        base = xglCdArcInitSub2(base);
        LW[0x32] = 1;
        while (xglCdArcInitSub1(&D_0093DC70, D_004D2478, base) <= 0)
            ;
        LW[0x32] = 0;
        p = xglCdArcInitSub2(base);
        base += ((int)(p - base) + 0x7FF) & -0x800;
    } else {
        while ((base += 0x2D000, xglCdArcInitSub1(&D_0093DC58, D_004D2468, base) <= 0) &&
               xglCdArcInitSub1(&D_0093DC58, D_004D2478, base) <= 0) {
            if (xglCdArcInitSub1(&D_0093DC58, D_004D2488, base) <= 0) {
                D_0093DC58.state = -1;
                base[0] = 0;
                base[1] = 0;
            }
            break;
        }
        base += 0x2000;
    }
    WorkEnd = base;
}

/*
 * ArcHeader is a four-entry table of 0x18-byte archive records (xglCdInitial
 * clears the state byte of entry 0..3 at +0x00/+0x18/+0x30/+0x48).  Only the
 * first two words of entry 0 are evidenced here: +0x00 is the state byte
 * (xglCdArcInit stores 2 there, xglCdInitial -1) and +0x04 is the work-buffer
 * pointer xglCdArcInit read the archive into (`sw WorkEnd, 0x4($18)` at
 * 0x0021F85C).  The rest of the record is not recovered, so the entry cannot
 * become a struct without inventing the span between them.
 */
#define ARC_HEADER_WORK_END_OFFSET 4

void xglCdArcCheck(void)
{
    u8 *saved_work_end = WorkEnd;

    WorkEnd = *(u8 **)(ArcHeader + ARC_HEADER_WORK_END_OFFSET);
    xglCdArcInit();
    WorkEnd = saved_work_end;
}

extern int sceCdReadDvdDualInfo(int *on_dual);
extern const char D_004D24D0[];
extern u8 system_cnf[];

void xglCdInitial(void)
{
    int dual;
    u8 *src;
    u8 *dst;
    int i;

    LW[0x31] = 0;
    LW[0x32] = 0;
    LW[0x33] = 0;
    LW[0x34] = 0;
    LW[0x1C] = 0;
    LW[0x1D] = 0;
    sceCdInit(0);
    sceCdMmode(2);
    xglCdReset();
    dual = 0;
    sceCdReadDvdDualInfo(&dual);
    LW[0x33] = dual;
    loaded_overlay = 2;
    ArcHeader[0] = -1;
    ArcHeader[0x18] = -1;
    ArcHeader[0x30] = -1;
    ArcHeader[0x48] = -1;
    src = WorkEnd;
    dst = system_cnf;
    xglCdReadFile(D_004D24D0, src, 0, 1);
    for (i = 0x4F; i >= 0; i--)
        *dst++ = *src++;
    xglHddActivate(0x100);
    xglHddMount();
    xglCdArcInit();
}

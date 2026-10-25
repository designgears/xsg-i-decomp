#ifndef MAIN_GAME_167_FILE_H
#define MAIN_GAME_167_FILE_H

#include "shared.h"

typedef struct SaveDataHeader {
    u64 version;
    u64 checksum;
} SaveDataHeader;

typedef unsigned char byte;

typedef union {
    byte data[0x163b0];
    struct {
        byte header[8];
        long long stored_checksum;
    } fields;
} FileChecksumData;

void FileVersionCheck(void);

void FileObjectDataClear(void);

extern byte *FileObjectData;

extern byte *FileJpegDec;
void FileCheckSumGet(FileChecksumData *source, long long *checksum);

int FileCheckSumCheck(FileChecksumData *data);

void FileSinkiSaveDataPush(void);

/*
 * TU-local declarations of game/167_file (main#167).
 */

int xglTaskWaitRemove(XglTaskPrefix *task);

/*
 * The file menu's work block: MenuFileMain takes 0x80 bytes, 0x80-aligned, from
 * the menu work area, clears them and stores the pointer in FileWork.
 */
typedef struct FileWorkBlock {
    unsigned char unmodeled_00[0]; /* +0x0: zero-length alias of the former padding member, kept for saved bodies */
    unsigned char input_flags; /* bit 0 enables the explanatory text */
    unsigned char command;
    unsigned char text_index;
    unsigned char state; /* 0xff ends the file menu */
    unsigned char unmodeled_04[1]; /* array kept: saved bodies index the former unmodeled_04[0x7c] */
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
} FileWorkBlock;

extern FileWorkBlock *FileWork;

#endif /* MAIN_GAME_167_FILE_H */

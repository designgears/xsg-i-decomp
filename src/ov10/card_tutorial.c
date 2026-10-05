/*
 * OV10 original TU 7: 0x00a21348..0x00a21768 (3 functions)
 */
#include "common.h"
#include "shared.h"

extern char *strcat(char *destination, const char *source);
extern char *strcpy(char *destination, const char *source);
extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);

/* "data\carddata\tuto\" */
extern char datapath[0x14];
/* Tutorial texture filenames, one per tutorial index. */
extern char *tutoxtx_tbl[];
/* ".xtx" */
extern const char D_00A4E498[];
/* The tutorial texture buffer xglCdReadFile loads the file into. */
extern void *tuxtx;

void CardTutorialFileLoad(int tutorialIndex)
{
    char path[0x80];

    memset(path, 0, sizeof(path));
    strcpy(path, datapath);
    strcat(path, tutoxtx_tbl[tutorialIndex]);
    strcat(path, D_00A4E498);
    xglCdReadFile(path, tuxtx, 0, 1);
}

typedef struct { char *name[2]; } TutoNames;
extern const TutoNames D_00A4E4B0;
extern const char D_00A4E4B8[];
extern const char D_00A4E4C8[];
extern int tutopage;
extern void *tuxtx_tbl;
extern void *tulex_tbl;
extern void *lex;

void CardTutorialInit(void)
{
    char path[0x80];
    TutoNames names;
    char *buf;
    int i;
    int size;

    buf = (char *)0x1A00000;
    tutopage = 0;
    lex = buf;
    names = D_00A4E4B0;
    memset(path, 0, sizeof(path));
    strcpy(path, datapath);
    strcat(path, D_00A4E4B8);
    size = xglCdReadFile(path, (void *)0x1A00000, 0, 1);
    if (size > 0) {
        buf = (char *)((size + 0x1A0000F) & ~0xF);
    }
    for (i = 0; i < 2 && names.name[i] != 0; i++) {
        memset(path, 0, sizeof(path));
        strcpy(path, datapath);
        strcat(path, names.name[i]);
        strcat(path, D_00A4E4C8);
        (&tulex_tbl)[i] = buf;
        size = xglCdReadFile(path, buf, 0, 1);
        if (size > 0) {
            buf += size;
            buf = (char *)((int)(buf + 0xF) & ~0xF);
        }
        memset(path, 0, sizeof(path));
        strcpy(path, datapath);
        strcat(path, names.name[i]);
        strcat(path, D_00A4E498);
        (&tuxtx_tbl)[i] = buf;
        size = xglCdReadFile(path, buf, 0, 1);
        if (size > 0) {
            buf += size;
            buf = (char *)((int)(buf + 0xF) & ~0xF);
        }
    }
    tuxtx = buf;
    CardTutorialFileLoad(0);
}

extern int printf(const char *format, ...);
extern void xglMatrixTrans(float dst[4][4], float src[4][4], float *vec);
extern void nmlModelSetTexture(void *texture);
extern void nmlModelSetPlace(float matrix[4][4]);
extern void nmlModelSetRenderLevel(int level);
extern void nmlModelEntry(void *model);
extern void xglSoundEffectNormalID(int id, int flag);
extern int tutopage;
extern void *tuxtx_tbl;
extern void *tulex_tbl;
extern void *lex;
typedef struct { long a; long b; } TutoVec16;
extern const TutoVec16 D_00A4E4D0;
extern const char D_00A4E4E0[];

int CardTutorialProc(void *work)
{
    float base[4][4];
    float place[4][4];
    TutoVec16 pos;

    pos = D_00A4E4D0;
    xglMatrixUnit(base);
    if (*(unsigned short *)((char *)work + 0x5AD2) & 8) {
        xglSoundEffectNormalID(1, 0);
        tutopage++;
        if (tutopage >= 0x2F) {
            printf(D_00A4E4E0);
            return 0;
        }
        if (tutoxtx_tbl[tutopage] == 0) {
            return 0;
        }
        if (((int (*)(int))CardTutorialFileLoad)(tutopage) < 0) {
            return 0;
        }
    }
    if (*(unsigned short *)((char *)work + 0x5AD2) & 4) {
        tutopage--;
        if (tutopage < 0) {
            tutopage = 0;
        } else {
            xglSoundEffectNormalID(2, 0);
            CardTutorialFileLoad(tutopage);
        }
    }
    if (*(unsigned short *)((char *)work + 0x5AD2) & 0x800) {
        return 0;
    }
    xglMatrixTrans(place, base, (float *)&pos);
    nmlModelSetTexture(tuxtx_tbl);
    nmlModelSetPlace(place);
    nmlModelSetRenderLevel(2);
    nmlModelEntry(tulex_tbl);
    xglMatrixUnit(base);
    base[3][2] = -1.0f;
    nmlModelSetPlace(base);
    nmlModelSetTexture(tuxtx);
    nmlModelEntry(lex);
    return 1;
}

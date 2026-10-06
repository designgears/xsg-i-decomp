#include "common.h"

typedef struct GameMovieMovieInfo {
    unsigned char pad_00[0x40];
    short state;
    unsigned char pad_42[4];
    unsigned short frame;
    unsigned char pad_48[0x83];
    unsigned char threshold0;
} GameMovieMovieInfo;
typedef struct { unsigned char pad[0x10]; int flags; } GameMovieLoopState;
typedef struct { unsigned char pad[0x14]; unsigned short packetControl; } GameMovieRender;
typedef struct { unsigned char pad[0x40]; unsigned long long w40; unsigned char pad48[0x28]; unsigned long long w70; } GameMovieEnv;
extern struct MovieInfo mi;
#define MI (*(GameMovieMovieInfo *)&mi)
extern GameMovieLoopState GameLoopState;
extern GameMovieRender sRender;
extern GameMovieEnv TestEnv_0_00367980;
extern unsigned char GameMovieAlpha;
extern unsigned short GameMovieFrame;
extern unsigned char GameMovieTransparent;
extern void xglMoviePlay(void *m);
extern void nmlModelDirectSend(int a, void *p, int n);

void GameMovieMain(void)
{
    if (MI.state != 0) {
        if (!(GameLoopState.flags & 1)) {
            MI.threshold0 = GameMovieTransparent;
            xglMoviePlay(&mi);
        }
        GameMovieFrame = MI.frame;
        TestEnv_0_00367980.w70 = ((unsigned long long)GameMovieAlpha << 32) | 0x64;
        TestEnv_0_00367980.w40 = (0x24020000 | (sRender.packetControl << 5)) | 0x2000000640000000ULL;
        nmlModelDirectSend(5, &TestEnv_0_00367980, 0xD);
    }
}

/* Defined in src/main/xgl_1.c, still INCLUDE_ASM there. */
extern int xglMovieClose(void *movie);

/*
 * Partial view of the movie handle xglMovieClose closes; only the
 * playback-state field GameMovieStop clears is modeled, the rest of the
 * layout is not recovered.
 */
typedef struct MovieInfo {
    unsigned char unmodeled_00[0x40];
    short state;                      /* 0x40 */
    unsigned char unmodeled_42[0x4e];
    void *source_data;                /* 0x90 */
    int source_size;                  /* 0x94 */
    unsigned char unmodeled_98[2];
    unsigned char opened;             /* 0x9a */
    unsigned char unmodeled_9b[0x15];
    void *buf0;                       /* 0xb0 */
    void *buf1;                       /* 0xb4 */
    unsigned char unmodeled_b8[0x11];
    unsigned char transMode;          /* 0xc9 */
    unsigned char threshold1;         /* 0xca */
    unsigned char threshold0;         /* 0xcb */
    unsigned char pad_cc[0x14];
} MovieInfo;

extern MovieInfo mi;

void GameMovieStop(void)
{
    xglMovieClose(&mi);
    mi.state = 0;
}

/* Defined in src/main/xgl_1.c, still INCLUDE_ASM there. */
extern int xglMovieOpen(void *movie, char *name);

int GameMoviePlay(char *name)
{
    if (mi.state != 0) {
        xglMovieClose(&mi);
    }

    return xglMovieOpen(&mi, name);
}

extern int SCRIPT_getEventActiveFlag(void);
extern int SCRIPT_getFadeTime(void);
extern void SCRIPT_sendMovieSkipSignal(void);
extern void nmlModelSetActiveFadeInCancel(int fadeTime);

static void movie_skip(int *mode)
{
    if (SCRIPT_getEventActiveFlag() != 0) {
        if (*mode != 1) {
            *mode = 2;
        }

        SCRIPT_sendMovieSkipSignal();
        nmlModelSetActiveFadeInCancel(SCRIPT_getFadeTime());
    }
}

static char *caption_nextline(char *p) {
    char c = *p;
    int u = (unsigned char)*p;
    char d;
    int t, cr;
    if (c == 13 || c == 10) goto l0;
    if (c != 0x3B) goto out;
    p++;
    u = (unsigned char)*p;
    for (;;) {
l0:
        cr = 13;
        d = u;
        p++;
        if (d != cr && d != 10) {
            u = (unsigned char)*p;
            continue;
        }
        t = (unsigned char)*p;
        u = t;
        d = (char)t;
        if (d == cr || d == 10 || d == 0x3B) continue;
        break;
    }
out:
    return p;
}

INCLUDE_ASM("asm/main/nonmatchings/game_movie", caption_getparam);

extern int xglFontGetStringWidth(const char *text);
extern char D_004C0290[];
char *caption_getparam(char *src, int *out, char *dst);

static void caption_convert(char *buf)
{
    char *src;
    int param[4];

    src = buf;
    xglFontGetStringWidth(D_004C0290);
    if (*buf == ';') {
        src = caption_nextline(buf);
    }
    *buf++ = 0;
    do {
        src = caption_getparam(src, param, buf + 2);
        buf[0] = param[0];
        buf[1] = param[0] >> 8;
        buf = buf + 2;
        while (*buf++ != 0) {
        }
    } while (param[0] >= 0);
}

INCLUDE_ASM("asm/main/nonmatchings/game_movie", GameMpeg2Play);

INCLUDE_ASM("asm/main/nonmatchings/game_movie", GameMovieInit);

/*
 * OV01 original TU 19: 0x00a32e78..0x00a332e0 (8 functions)
 */
#include "common.h"
#include "shared.h"

/*
 * .bss for this TU is still scaffold-owned (config/tu-build.json
 * data_ownership); these keep the scaffold's D_00XXXXXX names. The original
 * ELF's own local symbol table names the flag word mvFlags (config/symbols/
 * ov01.txt disambiguates the two same-named words at 0x00a43720/0x00a43724 as
 * mvFlags_00A43720/mvFlags_00A43724); the movie-parameter block and the movie
 * handle at 0x00a5b630/0x00a5b6d0 have no ledger entry and keep the splat
 * default.
 */
extern int mvFlags_00A43720;

#define MV_FLAG_INITIALIZED 0x1
#define MV_FLAG_PLAYING 0x2
#define MV_FLAG_SKIP_ENABLED 0x4
#define MV_PLAYING_BIT 1

/*
 * Partial view of the movie-parameter block MMvPlay2 sets up (still
 * INCLUDE_ASM in this TU); only the two fields MMvGetTime reads are modeled.
 */
typedef struct MvParams {
    unsigned char unmodeled_00[0xe4];
    short totalTime;   /* +0xe4 */
    short currentTime; /* +0xe6 */
} MvParams;

extern MvParams D_00A5B630;

/* Movie playback handle MMvPlay opens and MMvStop closes; still opaque. */
extern unsigned char D_00A5B6D0[];

typedef struct MvGifEnvironment {
    unsigned char unmodeled_00[0x30];
    u64 transfer_tag;
} MvGifEnvironment;

extern MvGifEnvironment mvEnv;

typedef struct MvRenderControl {
    unsigned char unmodeled_00[0x14];
    u16 movie_gif_control;
} MvRenderControl;

#define MV_RENDER_STATE ((MvRenderControl *) 0x004a90e0)
extern PadPrefix PadData;
extern void FlushCache(int mode);
extern void xglDmaDirectNormal(u32 channel, u32 address, u32 count);
extern int xglMoviePlay(void *movie);
void MMvStop(void);

/* Defined in src/main/xgl_1.c, still INCLUDE_ASM there. */
extern int xglMovieClose(void *movie);

typedef struct MvPlayOptions {
    int flags;
    const char *filename;
    int frame_x;
    int frame_y;
    int frame_width;
    int frame_height;
} MvPlayOptions;

int MMvPlay2(MvPlayOptions *options);

int MMvInit(void)
{
    mvFlags_00A43720 = MV_FLAG_INITIALIZED | MV_FLAG_SKIP_ENABLED;
    return 1;
}

extern MvRenderControl sRender;

void MMvExec(void)
{
    int done;

    if (mvFlags_00A43720 & MV_FLAG_PLAYING)
    {
        if ((mvFlags_00A43720 & MV_FLAG_SKIP_ENABLED) && (PadData.half_28 & 0x100) && (PadData.half_2a & 0x40))
        {
            MMvStop();
            return;
        }
        done = xglMoviePlay(D_00A5B6D0);
        mvEnv.transfer_tag = 0x24020000 | (sRender.movie_gif_control << 5) | 0x2000000640000000ULL;
        FlushCache(0);
        xglDmaDirectNormal(2, (u32)&mvEnv, 9);
        if (done != 0)
        {
            xglMovieClose(D_00A5B6D0);
            mvFlags_00A43720 &= ~MV_FLAG_PLAYING;
        }
    }
}

int MMvPlay(const char *filename)
{
    MvPlayOptions options;

    options.flags = 2;
    options.filename = filename;
    return MMvPlay2(&options);
}

extern void MOutputDebugStringWarn(const char *format, ...);
extern char *strncpy(char *dest, const char *src, unsigned int n);
extern void xglMovieInfoInit(void *info);
extern int xglMovieOpen(void *info, char *name);
typedef struct MvMovieHandle {
    unsigned char unmodeled_00[0x40];
    short width;              /* +0x40 */
    short height;             /* +0x42 */
    unsigned char unmodeled_44[4];
    int bufferSize;           /* +0x48 */
    unsigned char unmodeled_4c[0x44];
    void *arena;              /* +0x90 */
    int arenaSize : 32;            /* +0x94 */
    unsigned char unmodeled_98[0x18];
    void *buf0;               /* +0xb0 */
    void *buf1;               /* +0xb4 */
    unsigned char unmodeled_b8[0x11];
    unsigned char flag_c9;    /* +0xc9 */
    unsigned char unmodeled_ca[0xd0 - 0xca];
} MvMovieHandle;
typedef struct MvPlayState {
    MvPlayOptions options;    /* +0x00 */
    char filename[0x80];      /* +0x18 */
    unsigned char unmodeled_98[8];
    MvMovieHandle movie;      /* +0xa0 */
    void *arena;              /* +0x170 */
    void *buf0;               /* +0x174 */
    void *buf1;               /* +0x178 */
    unsigned char unmodeled_17c[4];
} MvPlayState;
typedef struct MvEnvView {
    unsigned char unmodeled_00[0x60];
    int x0 : 32;      /* +0x60 */
    int y0 : 32;      /* +0x64 */
    unsigned char unmodeled_68[8];
    int width : 32;   /* +0x70 */
    int height : 32;  /* +0x74 */
    unsigned char unmodeled_78[8];
    int x1 : 32;      /* +0x80 */
    int y1 : 32;      /* +0x84 */
} MvEnvView;

int MMvPlay2(MvPlayOptions *options)
{
    MvPlayState *st;
    MvEnvView *env;
    MvMovieHandle *mv;
    void *buf;
    void *arena = (void *)0x01D9E000;
    int h, w;
    int x, y;

    if (mvFlags_00A43720 & MV_FLAG_PLAYING) {
        MOutputDebugStringWarn("MMvPlay: Now playing, wait for the end of movie");
        return 0;
    }
    st = (MvPlayState *)&D_00A5B630;
    memset(st, 0, sizeof(MvPlayState));
    st->arena = arena;
    st->options = *options;
    buf = (void *)0x01E1E000;
    st->buf0 = buf;
    buf = (void *)0x01E2A000;
    st->buf1 = buf;
    strncpy(st->filename, options->filename, sizeof(st->filename));
    st->filename[sizeof(st->filename) - 1] = '\0';
    mv = &st->movie;
    xglMovieInfoInit(&st->movie);
    mv->arena = st->arena;
    mv->arenaSize = 0x40000;
    if (xglMovieOpen(mv, st->filename) != 0) {
        MOutputDebugStringWarn("MMvPlay: Failed to open '%s'", st->filename);
        return 0;
    }
    if (mv->bufferSize > 0xC00) {
        MOutputDebugStringWarn("MMvPlay: Could not play the movie, because");
        MOutputDebugStringWarn("MMvPlay: the size of the decode buffer is not sufficient");
        xglMovieClose(mv);
        return 0;
    }
    env = (MvEnvView *)&mvEnv;
    h = mv->height;
    w = mv->width;
    mv->buf0 = st->buf0;
    mv->buf1 = st->buf1;
    env->width = w << 4;
    env->height = h << 4;
    mv->flag_c9 = 0;
    if (st->options.flags & 1) {
        st->options.frame_width = w;
        st->options.frame_height = h;
    }
    if (st->options.flags & 2) {
        st->options.frame_width = 0x200;
        st->options.frame_height = 0x1C0;
        st->options.frame_x = 0;
        st->options.frame_y = 0;
    }
    x = st->options.frame_x;
    y = st->options.frame_y;
    env->x0 = x * 16 + 0x6FF8;
    env->y0 = y * 16 + 0x71F7;
    env->x1 = (x + st->options.frame_width) * 16 + 0x6FF8;
    env->y1 = (y + st->options.frame_height) * 16 + 0x71F7;
    mvFlags_00A43720 |= MV_FLAG_PLAYING;
    return 1;
}

void MMvStop(void)
{
    if (mvFlags_00A43720 & MV_FLAG_PLAYING)
    {
        xglMovieClose(D_00A5B6D0);
        mvFlags_00A43720 &= ~MV_FLAG_PLAYING;
    }
}

int MMvIsPlaying(void)
{
    return (mvFlags_00A43720 >> MV_PLAYING_BIT) & 1;
}

int MMvGetTime(int *currentTime, int *totalTime)
{
    short movieCurrentTime = 0;
    short movieTotalTime = 0;

    if (mvFlags_00A43720 & MV_FLAG_PLAYING)
    {
        movieCurrentTime = D_00A5B630.currentTime;
        movieTotalTime = D_00A5B630.totalTime;
    }

    if (currentTime != 0)
    {
        *currentTime = movieCurrentTime;
    }
    if (totalTime != 0)
    {
        *totalTime = movieTotalTime;
    }

    return movieTotalTime - movieCurrentTime;
}

void MMvSkipEnabled(short enable)
{
    if (enable != 0)
    {
        mvFlags_00A43720 |= MV_FLAG_SKIP_ENABLED;
    }
    else
    {
        mvFlags_00A43720 &= ~MV_FLAG_SKIP_ENABLED;
    }
}

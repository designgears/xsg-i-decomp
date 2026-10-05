#include "common.h"
#include "shared.h"

typedef struct MovieXtxHeader {
    u32 signature;
    u32 content[3];
} MovieXtxHeader;

/* The movie playback resource owns two contiguous XTX header sequences. */
typedef struct MoviePlaybackWork {
    unsigned char unmodeled_00[0x4c];
    unsigned int xtx_header_count;
    unsigned char unmodeled_50[0x40];
    void *source_data;
    int source_size;
    unsigned char unmodeled_98[2];
    unsigned char opened;
    unsigned char unmodeled_9b[0x15];
    MovieXtxHeader *first_header;
    MovieXtxHeader *second_header;
    MovieXtxHeader *next_header;
    MovieXtxHeader *xtx_data;
    unsigned char unmodeled_c0[0x10];
    MovieXtxHeader xtx_headers[0];
} MoviePlaybackWork;

extern void *GameResourceAlloc(int size);
extern void *GameResourceRealloc(void *resource, int size);
extern void xglMovieInfoInit(MoviePlaybackWork *info);
extern int xglMovieOpen(MoviePlaybackWork *info, int mode);
extern MovieXtxHeader *xglMovieMakeXtxHeader(MoviePlaybackWork *info, MovieXtxHeader *header);

typedef struct { float x, y, z; int pad; int r, g, b, col; int sx, sy, sz, q; } U6Vtx;
typedef struct { float m[16]; int x0, y0, z, pad4c, w, h, mode, col; float v[4]; float u0, v0, uw, vh; float fx, fy, fz[2], fw, fh; } U6Spr;
typedef struct { unsigned char p0[0x40]; unsigned long gs; unsigned char p48[0x48]; U6Vtx vtx[4]; } U6Env;
typedef struct { unsigned char p0[0xb8]; void *xtx[4]; unsigned char pc8; } U6Mdl;
typedef struct { unsigned char p0[0xa6]; short a6; unsigned char pa8[0x1a0-0xa8]; int x, y, w, h; int pad1b0; int mode; unsigned short uw, cols, vh; unsigned char p1be[2]; unsigned char col; unsigned char p1c1[0x1d0-0x1c1]; float z0, z1; unsigned char frame; unsigned char p1d9[2]; unsigned char kind; unsigned char p1dc[4]; void *mdl; } U6Work;
extern U6Env TestEnv_0_00490520;
extern unsigned char clut_1[];
extern struct { unsigned char p[0x1a]; unsigned short fbp; } sRender;
extern char D_004D2170[];
extern void nmlModelDirectSendXtx(int, void *);
extern void nmlModelDirectSend(int, void *, int);
extern float I2F(int value);
extern void xglMatrixStackSave(float (*)[4]);
extern float xglRotTransPers(void *, void *, void *, int);
extern int printf(const char *, ...);
#define SPR_I(o) (*(int *)(0x70000000 + (o)))
typedef union { float f; unsigned short s; unsigned char c; int i; } U6Any;
#define SPR_F(o) (((U6Any *)(0x70000000 + (o)))->f)

static void unit6003_draw(U6Work *p)
{
    int ctx;
    int m;
    int cam;
    int i;
    U6Vtx *v;
    float scale;
    float q;

    ctx = 5;
    if (p->a6 != 0) ctx = 1;
    switch (p->kind) {
    case 0:
        nmlModelDirectSendXtx(ctx, p->mdl);
        break;
    case 1:
        nmlModelDirectSendXtx(ctx, ((U6Mdl *)p->mdl)->xtx[((U6Mdl *)p->mdl)->pc8]);
        break;
    default:
        printf(D_004D2170, p->kind);
        break;
    }
    switch (p->kind) {
    case 0: {
        int f = sRender.fbp << 5;
        TestEnv_0_00490520.gs = 0x25320000 | f | ((unsigned long)(f + clut_1[p->frame] + 0x380) << 37) | ((((unsigned long)0x20000006 << 16) | 0x4000) << 16);
        break;
    }
    case 1:
        TestEnv_0_00490520.gs = 0x20010000 | (sRender.fbp << 5) | ((unsigned long)0x20000006 << 32);
        break;
    }
    cam = 0;
    v = TestEnv_0_00490520.vtx;
    while (cam < 8 && xglStudioGetCamera2(cam)->active == 0) cam++;
    switch (p->kind) {
    case 0:
        scale = 0.001953125f;
        SPR_F(0x70) = I2F((p->frame % p->cols) * p->uw) * scale;
        SPR_F(0x74) = I2F((p->frame / p->cols) * p->vh) * scale;
        goto common;
    case 1:
        SPR_F(0x70) = 0.0f;
        SPR_F(0x74) = 0.0f;
        scale = 0.00390625f;
    common:
        SPR_F(0x78) = I2F(p->uw) * scale;
        SPR_F(0x7c) = I2F(p->vh) * scale;
        break;
    }
    m = p->mode;
    SPR_I(0x58) = m;
    SPR_I(0x5c) = p->col;
    if (m == 0) {
        SPR_F(0x80) = *(float *)&p->x - *(float *)&p->w * 0.5f;
        SPR_F(0x84) = *(float *)&p->y + *(float *)&p->h * 0.5f;
        SPR_F(0x88) = p->z0;
        SPR_F(0x8c) = p->z1;
        SPR_F(0x90) = *(float *)&p->w;
        SPR_F(0x94) = *(float *)&p->h;
    } else {
        SPR_I(0x40) = (p->x << 4) + 0x6ff8;
        SPR_I(0x44) = (p->y << 4) + 0x71f8;
        SPR_I(0x48) = -1;
        SPR_I(0x50) = p->w << 4;
        SPR_I(0x54) = p->h << 4;
    }
    xglMatrixStackSave((float (*)[4])0x70000000);
    for (i = 0; i < 4; i++, v++) {
        int bx = i & 1;
        if (SPR_I(0x58) == 0) {
            SPR_F(0x60) = SPR_F(0x80) + (bx ? SPR_F(0x90) : 0.0f);
            SPR_F(0x64) = SPR_F(0x84) - ((i & 2) ? SPR_F(0x94) : 0.0f);
            SPR_F(0x68) = ((float *)0x70000088)[(i & 2) >> 1];
            SPR_F(0x6c) = 1.0f;
            q = xglRotTransPers(&v->sx, (void *)0x70000000, (void *)0x70000060, cam);
            v->q = 0;
            if ((v->sx & 0xffff0000) || (v->sy & 0xffff0000) || v->sz < 0) return;
        } else {
            v->sx = SPR_I(0x40) + (bx ? SPR_I(0x50) : 0);
            v->sy = SPR_I(0x44) + ((i & 2) ? SPR_I(0x54) : 0);
            q = 1.0f;
            {
                int t = SPR_I(0x48);
                v->q = 0;
                v->sz = t;
            }
        }
        v->x = (SPR_F(0x70) + (bx ? SPR_F(0x78) : 0.0f)) * q;
        v->y = (SPR_F(0x74) + ((i & 2) ? SPR_F(0x7c) : 0.0f)) * q;
        v->r = 0x80;
        v->g = 0x80;
        v->b = 0x80;
        v->z = q;
        v->col = SPR_I(0x5c);
    }
    nmlModelDirectSend(ctx, &TestEnv_0_00490520, 0x15);
}

static MoviePlaybackWork *movie_work_setup(void *source_data, int source_size)
{
    MoviePlaybackWork *info;
    MovieXtxHeader *header_cursor;
    int allocation_size;

    info = GameResourceAlloc(0xd0);
    xglMovieInfoInit(info);
    info->opened = 1;
    info->source_data = source_data;
    info->source_size = source_size;
    xglMovieOpen(info, 0);
    info->xtx_data = info->xtx_headers;
    header_cursor = xglMovieMakeXtxHeader(info, info->xtx_data);
    info->first_header = header_cursor;
    header_cursor += info->xtx_header_count;
    info->next_header = header_cursor;
    header_cursor = xglMovieMakeXtxHeader(info, header_cursor);
    info->second_header = header_cursor;
    header_cursor += info->xtx_header_count;
    allocation_size = (int)((unsigned char *)header_cursor - (unsigned char *)info);
    GameResourceRealloc(info, allocation_size);
    return info;
}

INCLUDE_ASM("asm/main/nonmatchings/unit6003", unit6003_update);

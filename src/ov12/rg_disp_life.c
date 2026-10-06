/*
 * OV12 original TU 43: 0x00a25d30..0x00a26a00 (12 functions)
 */
#include "common.h"
#include "rg_disp_life.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

extern void *RgHeapAlloc(RgHeap *heap, unsigned int size,
                         const char *source_file, int line);
extern void RgHeapFree(RgHeap *heap, void *ptr, const char *source_file,
                       int line);
extern RgHeap *InstanceOfRgHeap(void);

static void _InitDisp(RgDispLife *pDisp, int dispTex, int timeFont);
static void _DestructDisp(RgDispLife *pDisp);

/* The original TU-local tables and strings used by the C accessors below. */

struct AgwsNameUvwh;

typedef struct AgwsNameRecord {
    int characterId;
    unsigned char unmodeled_04[12];
    int texture_u;
    int texture_v;
    int texture_width;
    int texture_height;
} AgwsNameRecord;

static AgwsNameRecord s_aCharIDToUVWH_0[6] = {
    {0, {0}, 0, 0, 88, 16},
    {2, {0}, 168, 0, 80, 16},
    {4, {0}, 88, 0, 80, 16},
    {5, {0}, 0, 32, 88, 16},
    {3, {0}, 88, 32, 80, 16},
    {1, {0}, 168, 32, 80, 16}
};

const char D_00A54810[] = "pPic != NIL";
const char D_00A54820[] = "../rg_disp_life.euc.c";
const char D_00A54838[] = "pDisp != NIL";
const char D_00A54870[] = "board_na.bmp";
const char D_00A54880[] = "board_nb.bmp";
const char D_00A54950[] = "board_nc.bmp";
const char D_00A54960[] = "board_nd.bmp";

static int _GetAgwsNameUVWH(int nameIndex, struct AgwsNameUvwh *uvwh)
{
    unsigned int index;

    for (index = 0; index < 6; index++) {
        if (s_aCharIDToUVWH_0[index].characterId == nameIndex) {
            /*
             * Each record is 32 bytes, with an aligned 16-byte UVWH vector
             * at +0x10. VF31 carries those four words into the output object;
             * it is scratch VU state for this transfer.
             */
            __asm__ __volatile__(
                "lqc2 vf31, 0(%0)\n\t"
                "sqc2 vf31, 0(%1)\n\t"
                :
                : "r"(&s_aCharIDToUVWH_0[index].texture_u), "r"(uvwh)
                : "memory");
            return 1;
        }
    }
    return 0;
}

/*
 * The AGWS name texture's UV rectangle _GetAgwsNameUVWH (still INCLUDE_ASM)
 * fills: u, v feed XrgPaint2DSetUVOffset and w, h feed both
 * XrgPaint2DSetUVSize and the draw call's own width/height.
 */
typedef struct AgwsNameUvwh {
    int u;
    int v;
    int w;
    int h;
} AgwsNameUvwh;

static int _GetAgwsNameUVWH(int nameIndex, AgwsNameUvwh *uvwh);

extern void XrgPaint2DUseTexture(void *paint, void *pic);
extern void XrgPaint2DSetUVOffset(void *paint, int u, int v);
extern void XrgPaint2DSetUVSize(void *paint, int width, int height);
extern void XrgPaint2DAlpha(void *paint, int blendMode);
extern void XrgPaint2DDrawXYWH(void *paint, int mode, int x, int y,
                               int width, int height);

static void _DispAgwsName(void *paint, void *pPic, int nameIndex, int x,
                          int y)
{
    AgwsNameUvwh uvwh;

    if (pPic == 0) {
        assert_prog(D_00A54810, D_00A54820, 100);
    }
    if (_GetAgwsNameUVWH(nameIndex, &uvwh) && (pPic != 0)) {
        XrgPaint2DUseTexture(paint, pPic);
        XrgPaint2DSetUVOffset(paint, uvwh.u, uvwh.v);
        XrgPaint2DSetUVSize(paint, uvwh.w, uvwh.h);
        XrgPaint2DAlpha(paint, 0);
        XrgPaint2DDrawXYWH(paint, 0, x, y, uvwh.w, uvwh.h);
    }
}

extern const char D_00A54848[];
extern const char D_00A54858[];
extern const char D_00A54870[];
extern const char D_00A54880[];
extern const char D_00A54890[];
extern const char D_00A548A0[];
extern const char D_00A548B0[];
extern const char D_00A548C0[];
extern const char D_00A548D0[];
extern const char D_00A548E0[];
extern const char D_00A548F0[];
extern const char D_00A54900[];
extern const char D_00A54910[];
extern const char D_00A54920[];
extern const char D_00A54930[];
extern const char D_00A54940[];
extern void *RgBxxGetPic(struct RgBxx *bxx, const char *picture_name);
extern RgGauge *CreateRgGauge(void);
extern void *RgGaugeGetDisp(RgGauge *gauge);
extern void RgGaugeDispSetTex(void *gaugeDisp, void *barPic, void *restPic);
extern void RgGaugeDispSetBarUVWH(void *gaugeDisp, int x, int y, int width,
                                  int height);
extern void RgGaugeDispSetRestUVWH(void *gaugeDisp, int x, int y, int width,
                                   int height);
extern void RgGaugeDispSetPos(void *gaugeDisp, int x, int y);
extern void RgGaugeDispSetAlpha(void *gaugeDisp, int alpha);
extern void *CreateXrgPaint2D_sub(const char *source_file, int line);
extern void XrgPaint2DSetDrawPrio(void *paint, int prio);

static void _InitDisp(RgDispLife *pDisp, int dispTex, int timeFont)
{
    struct RgBxx *bxx;
    void *gaugeDisp;
    void *barPic;
    unsigned int i;

    if (pDisp == 0) {
        assert_prog(D_00A54838, D_00A54820, 0x75);
    }
    if (dispTex == 0) {
        assert_prog(D_00A54848, D_00A54820, 0x76);
    }
    if (timeFont == 0) {
        assert_prog(D_00A54858, D_00A54820, 0x77);
    }
    bxx = (struct RgBxx *)dispTex;
    *(int *)pDisp->unmodeled_04 = timeFont;
    pDisp->timer = 0.0f;
    pDisp->vsMode = 0;
    pDisp->dispBxx = bxx;
    pDisp->primaryBoardPic = (int)RgBxxGetPic(bxx, D_00A54870);
    pDisp->secondaryBoardPic = (int)RgBxxGetPic(bxx, D_00A54880);
    *(int *)pDisp->unmodeled_10 = (int)RgBxxGetPic(bxx, D_00A54890);
    ((int *)pDisp->unmodeled_24)[0] = (int)RgBxxGetPic(bxx, D_00A548A0);
    ((int *)pDisp->unmodeled_24)[1] = (int)RgBxxGetPic(bxx, D_00A548B0);
    ((int *)pDisp->unmodeled_24)[2] = (int)RgBxxGetPic(bxx, D_00A548C0);
    ((int *)pDisp->unmodeled_24)[3] = (int)RgBxxGetPic(bxx, D_00A548D0);
    ((int *)pDisp->unmodeled_24)[4] = (int)RgBxxGetPic(bxx, D_00A548E0);
    ((int *)pDisp->unmodeled_24)[5] = (int)RgBxxGetPic(bxx, D_00A548F0);
    ((int *)pDisp->unmodeled_24)[6] = (int)RgBxxGetPic(bxx, D_00A54900);
    ((int *)pDisp->unmodeled_24)[7] = (int)RgBxxGetPic(bxx, D_00A54910);
    *(int *)&pDisp->unmodeled_4c[0xc] = 0;
    pDisp->elapsedTime = 0.0f;
    for (i = 0; i < 2; i++) {
        pDisp->robots[i] = 0;
        pDisp->gauge[i] = CreateRgGauge();
        ((int *)pDisp->unmodeled_50)[i] = 0;
        gaugeDisp = RgGaugeGetDisp(pDisp->gauge[i]);
        if (i == 0) {
            barPic = RgBxxGetPic(bxx, D_00A54920);
            RgGaugeDispSetTex(gaugeDisp, barPic, RgBxxGetPic(bxx, D_00A54930));
            RgGaugeDispSetBarUVWH(gaugeDisp, 0xE, 8, 0xB7, 0xC);
            RgGaugeDispSetRestUVWH(gaugeDisp, 0, 0, 0, 0xC);
            RgGaugeDispSetAlpha(gaugeDisp, 1);
            RgGaugeDispSetPos(gaugeDisp, 0x8E, 0x18);
        } else {
            barPic = RgBxxGetPic(bxx, D_00A54940);
            RgGaugeDispSetTex(gaugeDisp, barPic, RgBxxGetPic(bxx, D_00A54930));
            RgGaugeDispSetBarUVWH(gaugeDisp, 0xB, 0xF, 0xB7, 0xC);
            RgGaugeDispSetRestUVWH(gaugeDisp, 0, 0, 0, 0xC);
            RgGaugeDispSetAlpha(gaugeDisp, 1);
            RgGaugeDispSetPos(gaugeDisp, 0xBB, 0x2F);
        }
    }
    pDisp->paint = CreateXrgPaint2D_sub(D_00A54820, 0xB5);
    XrgPaint2DSetDrawPrio(pDisp->paint, 4);
}

extern void DisposeRgGauge(RgGauge *gauge);
extern void DisposeXrgPaint2D_sub(void *paint, const char *source_file,
                                  int line);

static void _DestructDisp(RgDispLife *pDisp)
{
    unsigned int i;

    if (pDisp == 0) {
        assert_prog(D_00A54838, D_00A54820, 188);
    }
    for (i = 0; i < 2; i++) {
        DisposeRgGauge(pDisp->gauge[i]);
    }
    DisposeXrgPaint2D_sub(pDisp->paint, D_00A54820, 192);
}

RgDispLife *CreateRgDispLife(int dispTex, int timeFont)
{
    RgDispLife *pDisp;

    pDisp = RgHeapAlloc(InstanceOfRgHeap(), 0x60, D_00A54820, 0xC7);
    _InitDisp(pDisp, dispTex, timeFont);
    return pDisp;
}

void DisposeRgDispLife(RgDispLife *pDisp)
{
    if (pDisp == 0) {
        assert_prog(D_00A54838, D_00A54820, 0xCF);
    }
    _DestructDisp(pDisp);
    RgHeapFree(InstanceOfRgHeap(), pDisp, D_00A54820, 0xD1);
}

void RgDispLifeSetRobot(RgDispLife *pDisp, RgStatus *pRobot1, RgStatus *pRobot2)
{
    if (pDisp == 0) {
        assert_prog(D_00A54838, D_00A54820, 0xDB);
    }
    pDisp->robot1P = pRobot1;
    pDisp->robot2P = pRobot2;
}

void RgDispLifeSetTimer(RgDispLife *pDisp, float timer)
{
    if (pDisp == 0) {
        assert_prog(D_00A54838, D_00A54820, 0xE5);
    }
    if (timer >= 0.0f) {
        pDisp->timer = timer;
    }
}

static inline int _ClampWin(int v, int lo, int hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void RgDispLifeSetWin(RgDispLife *pDisp, int win1, int win2, int arg3)
{
    if (pDisp == 0) {
        assert_prog(D_00A54838, D_00A54820, 0xEE);
    }
    *(int *)((char *)pDisp + 0x50) = _ClampWin(win1, 0, 2);
    *(int *)((char *)pDisp + 0x54) = _ClampWin(win2, 0, 2);
    *(int *)((char *)pDisp + 0x58) = arg3;
}

extern void *RgBxxGetPic(struct RgBxx *bxx, const char *picture_name);
extern const char D_00A54950[];
extern const char D_00A54960[];
extern const char D_00A54870[];
extern const char D_00A54880[];

void RgDispLifeSetVsMode(RgDispLife *pDisp, int vsMode)
{
    if (pDisp == 0) {
        assert_prog(D_00A54838, D_00A54820, 250);
    }

    pDisp->vsMode = vsMode;
    if (vsMode != 0) {
        pDisp->primaryBoardPic = RgBxxGetPic(pDisp->dispBxx, D_00A54950);
        pDisp->secondaryBoardPic = RgBxxGetPic(pDisp->dispBxx, D_00A54960);
    } else {
        pDisp->primaryBoardPic = RgBxxGetPic(pDisp->dispBxx, D_00A54870);
        pDisp->secondaryBoardPic = RgBxxGetPic(pDisp->dispBxx, D_00A54880);
    }
}

extern float RgRobotGetLife(RgStatus *robot);
extern void RgGaugeSetValue(RgGauge *gauge, float value);
extern void RgGaugePassTime(RgGauge *gauge, float deltaTime);

void RgDispLifePassTime(RgDispLife *pDisp, float deltaTime)
{
    unsigned int i;

    if (pDisp == 0) {
        assert_prog(D_00A54838, D_00A54820, 272);
    }
    for (i = 0; i < 2; i++) {
        if (pDisp->robots[i] != 0) {
            RgGaugeSetValue(pDisp->gauge[i], RgRobotGetLife(pDisp->robots[i]));
        }
        RgGaugePassTime(pDisp->gauge[i], deltaTime);
    }
    pDisp->elapsedTime += deltaTime;
    if (pDisp->elapsedTime > 1.0f) {
        pDisp->elapsedTime = 0.0f;
    }
}

extern float floorf(float value);
extern int RgFontStr(void *paint, int font_id, const char *text, int x, int y,
                     int right_align);
extern void *RgRobotGetSpec(RgStatus *pRobot);
extern void RgGaugeDraw(RgGauge *pGauge, void *pPaint);
extern void XrgPaint2DFlush(void *paint);
typedef struct { int x; int y; } RgDispLifePos;
extern const int s_anPos_1[4];
typedef struct RgDispLifeView {
    unsigned char pad00[0x24];
    void *lifeTex[2][3];  /* +0x24 */
    void *winBg[2];       /* +0x3c */
    unsigned char pad44[0x0c];
    int win[2];           /* +0x50 */
    int blink;            /* +0x58 */
    float elapsedTime;    /* +0x5c */
} RgDispLifeView;

void RgDispLifeDisp(RgDispLife *pDisp)
{
    RgDispLifeView *v = (RgDispLifeView *)pDisp;
    void *paint;
    char str[24];
    unsigned int t;
    unsigned int m;
    unsigned int i;
    void *texOff;
    void *texOn;
    int n;
    int k;

    if (pDisp == 0) {
        assert_prog(D_00A54838, D_00A54820, 0x125);
    }
    paint = pDisp->paint;
    if (pDisp->secondaryBoardPic != 0) {
        XrgPaint2DUseTexture(paint, pDisp->secondaryBoardPic);
        XrgPaint2DAlpha(paint, 0);
        XrgPaint2DDrawXYWH(paint, 0x20, 0, 0x10, 0, 0);
    }
    if (pDisp->primaryBoardPic != 0) {
        XrgPaint2DUseTexture(paint, pDisp->primaryBoardPic);
        XrgPaint2DAlpha(paint, 0);
        XrgPaint2DDrawXYWH(paint, 0x20, 0, 0, 0, 0);
    }
    t = floorf(pDisp->timer * 100.0f);
    str[0] = t / 60000 % 10 + '0';
    str[1] = t / 6000 % 10 + '0';
    str[2] = '\'';
    t = t % 6000;
    str[3] = t / 1000 % 10 + '0';
    str[4] = t / 100 % 10 + '0';
    str[5] = '"';
    str[6] = t / 10 % 10 + '0';
    str[7] = t % 10 + '0';
    str[8] = 0;
    RgFontStr(paint, *(int *)pDisp->unmodeled_04, str, 0x3E, 0x3D, 0);
    if (*(void **)pDisp->unmodeled_10 != 0) {
        for (i = 0; i < 2; i++) {
            if (pDisp->robots[i] != 0) {
                int x, y;
                int id = *(int *)RgRobotGetSpec(pDisp->robots[i]);
                x = 0x196;
                y = 0x14;
                if (i == 0) {
                    x = 0x1A;
                } else {
                    y = 0x2E;
                }
                _DispAgwsName(pDisp->paint, *(void **)pDisp->unmodeled_10, id, x, y);
            }
        }
    }
    for (i = 0; i < 2; i++) {
        RgGaugeDraw(pDisp->gauge[i], paint);
    }
    for (i = 0; i < 2; i++) {
        n = v->win[i];
        texOff = v->lifeTex[i][n];
        k = _ClampWin(n - 1, 0, 2);
        texOn = v->lifeTex[i][k];
        XrgPaint2DUseTexture(paint, v->winBg[i]);
        XrgPaint2DAlpha(paint, 0);
        XrgPaint2DDrawXYWH(paint, 0x20, s_anPos_1[i * 2], s_anPos_1[i * 2 + 1], 0, 0);
        m = v->blink & (1 << i);
        if (m != 0 && v->elapsedTime > 0.5f) {
            XrgPaint2DUseTexture(paint, texOn);
        } else {
            XrgPaint2DUseTexture(paint, texOff);
        }
        XrgPaint2DAlpha(paint, 0);
        XrgPaint2DDrawXYWH(paint, 0x20, s_anPos_1[i * 2], s_anPos_1[i * 2 + 1], 0, 0);
    }
    XrgPaint2DFlush(paint);
}

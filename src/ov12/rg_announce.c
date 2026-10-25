/*
 * OV12 original TU 77: 0x00a434b8..0x00a44f08 (35 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_announce.h"

extern void assert_prog(const char *expression, const char *source_file,
                        int line);

/*
 * External file-backed witnesses, not candidate-emitted data: this window is
 * asm-owned scaffold data (splat names, no config/symbols/ov12.txt entry).
 *
 * ov12:0x00a57bc0 contains the source filename "../rg_announce.euc.c".
 * ov12:0x00a57bb0 contains the assertion expression "pAnn != NIL".
 */
const char D_00A57BB0[] = "pAnn != NIL";
const char D_00A57BC0[] = "../rg_announce.euc.c";

extern void XrgPaint2DUseTexture(XrgPaint2D *paint, int texture);
extern void XrgPaint2DAlpha(XrgPaint2D *paint, int blend);
extern void XrgPaint2DDrawXYWH(XrgPaint2D *paint, int mode, int x, int y,
                               int width, int height);
extern void XrgPaint2DSetUVOffset(XrgPaint2D *paint, int u, int v);
extern void XrgPaint2DSetUVSize(XrgPaint2D *paint, int width, int height);
extern void XrgPaint2DFlush(XrgPaint2D *paint);
extern void DisposeXrgPaint2D_sub(XrgPaint2D *paint, const char *source_file,
                                  int line);
extern void *RgHeapAlloc(void *heap, unsigned int size,
                         const char *source_file, int line);
extern RgHeap *InstanceOfRgHeap(void);

static void _InitAnn(RgAnnounce *pAnn);

typedef signed int s32;
#define NULL 0 /* draft_fix */
extern const char D_00A57BD8[];
extern const char D_00A57BE8[];
extern const char D_00A57C10[];
extern const char D_00A57C28[];
extern const char D_00A57C38[];
extern const char D_00A57C60[];
extern const char D_00A57C70[];
extern const char D_00A57C98[];
extern const char D_00A57CA0[];
extern const char D_00A57CC8[];
extern const char D_00A57CD0[];
extern const char D_00A57CF8[];
extern const char D_00A57D08[];
extern const char D_00A57D30[];
extern const char D_00A57D40[];
extern const char D_00A57D68[];
extern const char D_00A57D70[];
extern const char D_00A57D98[];
extern const char D_00A57DA8[];
extern const char D_00A57DD0[];
extern const char D_00A57DE0[];
extern const char D_00A57E08[];
extern const char D_00A57E18[];
extern const char D_00A57E40[];
extern const char D_00A57E50[];
extern const char D_00A57E78[];
extern const char D_00A57E88[];
extern const char D_00A57EB0[];
extern const char D_00A57EC0[];
extern const char D_00A57EE8[];
extern const char D_00A57EF8[];
extern const char D_00A57F20[];
extern const char D_00A57F30[];
extern const char D_00A57F58[];
extern const char D_00A57F68[];
extern const char D_00A57F90[];
extern const char D_00A57FA0[];
extern const char D_00A57FC8[];
extern const char D_00A57FD8[];
extern const char D_00A58000[];
extern const char D_00A58010[];
extern const char D_00A58038[];
extern const char D_00A58048[];
extern const char D_00A58070[];
extern const char D_00A58080[];
extern const char D_00A580A8[];
extern const char D_00A580B8[];
extern const char D_00A580E0[];
extern const char D_00A580E8[];
extern const char D_00A58110[];
extern const char D_00A58120[];
extern const char D_00A58148[];
extern const char D_00A58158[];
extern const char D_00A58180[];
extern const char D_00A58188[];
extern const char D_00A581B0[];
extern const char D_00A581C0[];
extern const char D_00A581E8[];
extern const char D_00A581F8[];
extern const char D_00A58220[];
extern const char D_00A58230[];
extern const char D_00A58258[];
extern const char D_00A58268[];
extern const char D_00A58290[];
extern const char D_00A582A0[];
extern const char D_00A582C8[];
extern const char D_00A582D8[];
extern const char D_00A58300[];
extern const char D_00A58310[];
extern const char D_00A58338[];
extern const char D_00A58348[];
extern const char D_00A58370[];
extern const char D_00A58380[];
extern const char D_00A583A8[];
extern const char D_00A583B8[];
extern const char D_00A583E0[];
extern const char D_00A583F0[];
extern const char D_00A58418[];
extern const char D_00A58420[];
extern const char D_00A58448[];
extern const char D_00A58450[];
extern const char D_00A58478[];
extern const char D_00A58488[];
extern const char D_00A584B0[];
extern const char D_00A584C0[];
extern const char D_00A584E8[];
extern const char D_00A584F0[];
extern const char D_00A58518[];
extern const char D_00A58528[];
extern const char D_00A58550[];
extern const char D_00A58560[];
extern const char D_00A58588[];
extern const char D_00A58598[];
extern const char D_00A585C0[];
extern const char D_00A585D0[];
extern const char D_00A585F8[];
extern const char D_00A58608[];
extern const char D_00A58630[];
extern const char D_00A58640[];
extern const char D_00A58668[];
extern const char D_00A58678[];

static void _InitAnn(RgAnnounce *pAnn) {
    XrgPaint2D *temp_2_52;
    s32 temp_16;
    s32 temp_18;
    s32 temp_2_10;
    s32 temp_2_11;
    s32 temp_2_12;
    s32 temp_2_13;
    s32 temp_2_14;
    s32 temp_2_15;
    s32 temp_2_16;
    s32 temp_2_17;
    s32 temp_2_18;
    s32 temp_2_19;
    s32 temp_2_20;
    s32 temp_2_21;
    s32 temp_2_22;
    s32 temp_2_23;
    s32 temp_2_24;
    s32 temp_2_25;
    s32 temp_2_26;
    s32 temp_2_27;
    s32 temp_2_28;
    s32 temp_2_29;
    s32 temp_2_2;
    s32 temp_2_30;
    s32 temp_2_31;
    s32 temp_2_32;
    s32 temp_2_33;
    s32 temp_2_34;
    s32 temp_2_35;
    s32 temp_2_36;
    s32 temp_2_37;
    s32 temp_2_38;
    s32 temp_2_39;
    s32 temp_2_3;
    s32 temp_2_40;
    s32 temp_2_41;
    s32 temp_2_42;
    s32 temp_2_43;
    s32 temp_2_44;
    s32 temp_2_45;
    s32 temp_2_46;
    s32 temp_2_47;
    s32 temp_2_48;
    s32 temp_2_49;
    s32 temp_2_4;
    s32 temp_2_50;
    s32 temp_2_51;
    s32 temp_2_5;
    s32 temp_2_6;
    s32 temp_2_7;
    s32 temp_2_8;
    s32 temp_2_9;
    void *temp_2;

    if (pAnn == NULL) {
        assert_prog(D_00A57BB0, D_00A57BC0, 0x4C);
    }
    temp_2 = InstanceOfRgBattleCommonData();
    temp_18 = RgBattleCommonDataAnnounceAdd(temp_2);
    temp_16 = RgBattleCommonDataAnnounceSub(temp_2);
    temp_2_2 = RgBxxGetPic((void *) temp_18, D_00A57BD8);
    pAnn->textures[0] = temp_2_2;
    if (temp_2_2 == 0) {
        assert_prog(D_00A57BE8, D_00A57C10, 1);
    }
    temp_2_3 = RgBxxGetPic((void *) temp_18, D_00A57C28);
    pAnn->textures[1] = temp_2_3;
    if (temp_2_3 == 0) {
        assert_prog(D_00A57C38, D_00A57C10, 2);
    }
    temp_2_4 = RgBxxGetPic((void *) temp_18, D_00A57C60);
    pAnn->textures[2] = temp_2_4;
    if (temp_2_4 == 0) {
        assert_prog(D_00A57C70, D_00A57C10, 3);
    }
    temp_2_5 = RgBxxGetPic((void *) temp_18, D_00A57C98);
    pAnn->textures[3] = temp_2_5;
    if (temp_2_5 == 0) {
        assert_prog(D_00A57CA0, D_00A57C10, 4);
    }
    temp_2_6 = RgBxxGetPic((void *) temp_18, D_00A57CC8);
    pAnn->textures[4] = temp_2_6;
    if (temp_2_6 == 0) {
        assert_prog(D_00A57CD0, D_00A57C10, 5);
    }
    temp_2_7 = RgBxxGetPic((void *) temp_18, D_00A57CF8);
    pAnn->textures[5] = temp_2_7;
    if (temp_2_7 == 0) {
        assert_prog(D_00A57D08, D_00A57C10, 6);
    }
    temp_2_8 = RgBxxGetPic((void *) temp_18, D_00A57D30);
    pAnn->textures[6] = temp_2_8;
    if (temp_2_8 == 0) {
        assert_prog(D_00A57D40, D_00A57C10, 7);
    }
    temp_2_9 = RgBxxGetPic((void *) temp_18, D_00A57D68);
    pAnn->textures[7] = temp_2_9;
    if (temp_2_9 == 0) {
        assert_prog(D_00A57D70, D_00A57C10, 8);
    }
    temp_2_10 = RgBxxGetPic((void *) temp_18, D_00A57D98);
    pAnn->textures[8] = temp_2_10;
    if (temp_2_10 == 0) {
        assert_prog(D_00A57DA8, D_00A57C10, 9);
    }
    temp_2_11 = RgBxxGetPic((void *) temp_18, D_00A57DD0);
    pAnn->textures[9] = temp_2_11;
    if (temp_2_11 == 0) {
        assert_prog(D_00A57DE0, D_00A57C10, 0xA);
    }
    temp_2_12 = RgBxxGetPic((void *) temp_18, D_00A57E08);
    pAnn->textures[0xA] = temp_2_12;
    if (temp_2_12 == 0) {
        assert_prog(D_00A57E18, D_00A57C10, 0xB);
    }
    temp_2_13 = RgBxxGetPic((void *) temp_18, D_00A57E40);
    pAnn->textures[0xB] = temp_2_13;
    if (temp_2_13 == 0) {
        assert_prog(D_00A57E50, D_00A57C10, 0xC);
    }
    temp_2_14 = RgBxxGetPic((void *) temp_18, D_00A57E78);
    pAnn->textures[0xC] = temp_2_14;
    if (temp_2_14 == 0) {
        assert_prog(D_00A57E88, D_00A57C10, 0xD);
    }
    temp_2_15 = RgBxxGetPic((void *) temp_18, D_00A57EB0);
    pAnn->textures[0xD] = temp_2_15;
    if (temp_2_15 == 0) {
        assert_prog(D_00A57EC0, D_00A57C10, 0xE);
    }
    temp_2_16 = RgBxxGetPic((void *) temp_18, D_00A57EE8);
    pAnn->textures[0xE] = temp_2_16;
    if (temp_2_16 == 0) {
        assert_prog(D_00A57EF8, D_00A57C10, 0xF);
    }
    temp_2_17 = RgBxxGetPic((void *) temp_18, D_00A57F20);
    pAnn->textures[0xF] = temp_2_17;
    if (temp_2_17 == 0) {
        assert_prog(D_00A57F30, D_00A57C10, 0x10);
    }
    temp_2_18 = RgBxxGetPic((void *) temp_18, D_00A57F58);
    pAnn->textures[0x10] = temp_2_18;
    if (temp_2_18 == 0) {
        assert_prog(D_00A57F68, D_00A57C10, 0x11);
    }
    temp_2_19 = RgBxxGetPic((void *) temp_18, D_00A57F90);
    pAnn->textures[0x11] = temp_2_19;
    if (temp_2_19 == 0) {
        assert_prog(D_00A57FA0, D_00A57C10, 0x12);
    }
    temp_2_20 = RgBxxGetPic((void *) temp_18, D_00A57FC8);
    pAnn->textures[0x12] = temp_2_20;
    if (temp_2_20 == 0) {
        assert_prog(D_00A57FD8, D_00A57C10, 0x13);
    }
    temp_2_21 = RgBxxGetPic((void *) temp_18, D_00A58000);
    pAnn->textures[0x13] = temp_2_21;
    if (temp_2_21 == 0) {
        assert_prog(D_00A58010, D_00A57C10, 0x14);
    }
    temp_2_22 = RgBxxGetPic((void *) temp_18, D_00A58038);
    pAnn->textures[0x14] = temp_2_22;
    if (temp_2_22 == 0) {
        assert_prog(D_00A58048, D_00A57C10, 0x15);
    }
    temp_2_23 = RgBxxGetPic((void *) temp_18, D_00A58070);
    pAnn->textures[0x15] = temp_2_23;
    if (temp_2_23 == 0) {
        assert_prog(D_00A58080, D_00A57C10, 0x16);
    }
    temp_2_24 = RgBxxGetPic((void *) temp_18, D_00A580A8);
    pAnn->textures[0x16] = temp_2_24;
    if (temp_2_24 == 0) {
        assert_prog(D_00A580B8, D_00A57C10, 0x17);
    }
    temp_2_25 = RgBxxGetPic((void *) temp_18, D_00A580E0);
    pAnn->textures[0x17] = temp_2_25;
    if (temp_2_25 == 0) {
        assert_prog(D_00A580E8, D_00A57C10, 0x18);
    }
    temp_2_26 = RgBxxGetPic((void *) temp_18, D_00A58110);
    pAnn->textures[0x18] = temp_2_26;
    if (temp_2_26 == 0) {
        assert_prog(D_00A58120, D_00A57C10, 0x19);
    }
    temp_2_27 = RgBxxGetPic((void *) temp_18, D_00A58148);
    pAnn->textures[0x19] = temp_2_27;
    if (temp_2_27 == 0) {
        assert_prog(D_00A58158, D_00A57C10, 0x1A);
    }
    temp_2_28 = RgBxxGetPic((void *) temp_18, D_00A58180);
    pAnn->textures[0x1A] = temp_2_28;
    if (temp_2_28 == 0) {
        assert_prog(D_00A58188, D_00A57C10, 0x1B);
    }
    temp_2_29 = RgBxxGetPic((void *) temp_16, D_00A581B0);
    pAnn->textures[0x1B] = temp_2_29;
    if (temp_2_29 == 0) {
        assert_prog(D_00A581C0, D_00A57C10, 0x1D);
    }
    temp_2_30 = RgBxxGetPic((void *) temp_16, D_00A581E8);
    pAnn->textures[0x1C] = temp_2_30;
    if (temp_2_30 == 0) {
        assert_prog(D_00A581F8, D_00A57C10, 0x1E);
    }
    temp_2_31 = RgBxxGetPic((void *) temp_16, D_00A58220);
    pAnn->textures[0x1D] = temp_2_31;
    if (temp_2_31 == 0) {
        assert_prog(D_00A58230, D_00A57C10, 0x1F);
    }
    temp_2_32 = RgBxxGetPic((void *) temp_16, D_00A58258);
    pAnn->textures[0x1E] = temp_2_32;
    if (temp_2_32 == 0) {
        assert_prog(D_00A58268, D_00A57C10, 0x20);
    }
    temp_2_33 = RgBxxGetPic((void *) temp_16, D_00A58290);
    pAnn->textures[0x1F] = temp_2_33;
    if (temp_2_33 == 0) {
        assert_prog(D_00A582A0, D_00A57C10, 0x21);
    }
    temp_2_34 = RgBxxGetPic((void *) temp_16, D_00A582C8);
    pAnn->textures[0x20] = temp_2_34;
    if (temp_2_34 == 0) {
        assert_prog(D_00A582D8, D_00A57C10, 0x22);
    }
    temp_2_35 = RgBxxGetPic((void *) temp_16, D_00A58300);
    pAnn->textures[0x21] = temp_2_35;
    if (temp_2_35 == 0) {
        assert_prog(D_00A58310, D_00A57C10, 0x23);
    }
    temp_2_36 = RgBxxGetPic((void *) temp_16, D_00A58338);
    pAnn->textures[0x22] = temp_2_36;
    if (temp_2_36 == 0) {
        assert_prog(D_00A58348, D_00A57C10, 0x24);
    }
    temp_2_37 = RgBxxGetPic((void *) temp_16, D_00A58370);
    pAnn->textures[0x23] = temp_2_37;
    if (temp_2_37 == 0) {
        assert_prog(D_00A58380, D_00A57C10, 0x25);
    }
    temp_2_38 = RgBxxGetPic((void *) temp_16, D_00A583A8);
    pAnn->textures[0x24] = temp_2_38;
    if (temp_2_38 == 0) {
        assert_prog(D_00A583B8, D_00A57C10, 0x26);
    }
    temp_2_39 = RgBxxGetPic((void *) temp_16, D_00A583E0);
    pAnn->textures[0x25] = temp_2_39;
    if (temp_2_39 == 0) {
        assert_prog(D_00A583F0, D_00A57C10, 0x27);
    }
    temp_2_40 = RgBxxGetPic((void *) temp_16, D_00A58418);
    pAnn->textures[0x26] = temp_2_40;
    if (temp_2_40 == 0) {
        assert_prog(D_00A58420, D_00A57C10, 0x28);
    }
    temp_2_41 = RgBxxGetPic((void *) temp_16, D_00A58448);
    pAnn->textures[0x27] = temp_2_41;
    if (temp_2_41 == 0) {
        assert_prog(D_00A58450, D_00A57C10, 0x29);
    }
    temp_2_42 = RgBxxGetPic((void *) temp_16, D_00A58478);
    pAnn->textures[0x28] = temp_2_42;
    if (temp_2_42 == 0) {
        assert_prog(D_00A58488, D_00A57C10, 0x2A);
    }
    temp_2_43 = RgBxxGetPic((void *) temp_16, D_00A584B0);
    pAnn->textures[0x29] = temp_2_43;
    if (temp_2_43 == 0) {
        assert_prog(D_00A584C0, D_00A57C10, 0x2B);
    }
    temp_2_44 = RgBxxGetPic((void *) temp_16, D_00A584E8);
    pAnn->textures[0x2A] = temp_2_44;
    if (temp_2_44 == 0) {
        assert_prog(D_00A584F0, D_00A57C10, 0x2C);
    }
    temp_2_45 = RgBxxGetPic((void *) temp_16, D_00A58518);
    pAnn->textures[0x2B] = temp_2_45;
    if (temp_2_45 == 0) {
        assert_prog(D_00A58528, D_00A57C10, 0x2D);
    }
    temp_2_46 = RgBxxGetPic((void *) temp_16, D_00A58550);
    pAnn->textures[0x2C] = temp_2_46;
    if (temp_2_46 == 0) {
        assert_prog(D_00A58560, D_00A57C10, 0x2E);
    }
    temp_2_47 = RgBxxGetPic((void *) temp_16, D_00A58588);
    pAnn->textures[0x2D] = temp_2_47;
    if (temp_2_47 == 0) {
        assert_prog(D_00A58598, D_00A57C10, 0x2F);
    }
    temp_2_48 = RgBxxGetPic((void *) temp_16, D_00A585C0);
    pAnn->textures[0x2E] = temp_2_48;
    if (temp_2_48 == 0) {
        assert_prog(D_00A585D0, D_00A57C10, 0x30);
    }
    temp_2_49 = RgBxxGetPic((void *) temp_16, D_00A585F8);
    pAnn->textures[0x2F] = temp_2_49;
    if (temp_2_49 == 0) {
        assert_prog(D_00A58608, D_00A57C10, 0x31);
    }
    temp_2_50 = RgBxxGetPic((void *) temp_16, D_00A58630);
    pAnn->textures[0x30] = temp_2_50;
    if (temp_2_50 == 0) {
        assert_prog(D_00A58640, D_00A57C10, 0x32);
    }
    temp_2_51 = RgBxxGetPic((void *) temp_16, D_00A58668);
    pAnn->textures[0x31] = temp_2_51;
    if (temp_2_51 == 0) {
        assert_prog(D_00A58678, D_00A57C10, 0x33);
    }
    pAnn->timer = 1e8f;
    temp_2_52 = CreateXrgPaint2D_sub(D_00A57BC0, 0x5B);
    pAnn->paint = temp_2_52;
    XrgPaint2DSetDrawPrio(temp_2_52, 5);
    pAnn->drawFunc = NULL;
    pAnn->gameClearActive = 0;
    pAnn->pendingDispY = 0x99;
    pAnn->stageClearActive = 0;
    pAnn->dispY = 0x99;
}
/* Warning: struct XrgPaint2D is not defined (only forward-declared) */

static void _DestructAnn(RgAnnounce *pAnn)
{
    if (pAnn == 0) {
        assert_prog(D_00A57BB0, D_00A57BC0, 102);
    }
    DisposeXrgPaint2D_sub(pAnn->paint, D_00A57BC0, 103);
}

RgAnnounce *CreateRgAnnounce(void)
{
    RgAnnounce *pAnn;

    pAnn = RgHeapAlloc(InstanceOfRgHeap(), RG_ANNOUNCE_SIZE, D_00A57BC0, 110);
    _InitAnn(pAnn);
    return pAnn;
}

extern void RgHeapFree(RgHeap *heap, void *ptr, const char *source_file,
                       int line);

void DisposeRgAnnounce(RgAnnounce *pAnn)
{
    if (pAnn == 0) {
        assert_prog(D_00A57BB0, D_00A57BC0, 118);
    }
    _DestructAnn(pAnn);
    RgHeapFree(InstanceOfRgHeap(), pAnn, D_00A57BC0, 120);
}

void RgAnnouncePassTime(RgAnnounce *pAnn, float deltaTime)
{
    if (pAnn == 0) {
        assert_prog(D_00A57BB0, D_00A57BC0, 129);
    }
    if (pAnn->timer != RG_ANNOUNCE_TIMER_INFINITE) {
        pAnn->timer -= deltaTime;
        if (pAnn->timer < 0.0f) {
            pAnn->timer = 0.0f;
            pAnn->drawFunc = 0;
        }
    }
}

typedef struct Pal { long long a, b; } Pal;
static inline int _clamp_lim(int lim, int t)
{
    return (t > lim) ? lim : t;
}
extern const Pal D_00A586A0;
extern void XrgPaint2DColor(XrgPaint2D *paint, const void *color);
static void _time_disp_title(RgAnnounce *pAnn, int x, int y);
static void _damage_disp_title(RgAnnounce *pAnn, int x, int y);
static void _time_disp(RgAnnounce *pAnn, int x, int y, unsigned int time);
static void _damage_disp(RgAnnounce *pAnn, int x, int y, float damage);
static void _paint_flush(RgAnnounce *pAnn);

void RgAnnounceDisp(RgAnnounce *pAnn)
{
  int tick;
  int new_var;
  int var;
  int t;
  int new_var4;
  unsigned int i;
  int y;
  int stageColor[4];
  int gameColor[4];
  int *new_var3;
  float *new_var2;
  unsigned int totalTime;
  float totalDamage;
  Pal pal;
  if (pAnn == 0)
  {
    assert_prog(D_00A57BB0, D_00A57BC0, 0x9A);
  }
  if (pAnn->drawFunc != 0)
  {
    pAnn->drawFunc(pAnn);
  }
  if (pAnn->stageClearActive != 0)
  {
    XrgPaint2D *paint = pAnn->paint;
    int sv;
    int slim;
    int st;
    int stick;
    memset(stageColor, 0, 0x10);
    slim = 0x50;
    stageColor[3] = slim;
    stick = pAnn->stageClearTick + 1;
    st = stick * 8;
    sv = 0;
    pAnn->stageClearTick = stick;
    if (st >= 0)
    {
      sv = _clamp_lim(slim, st);
    }
    stageColor[3] = sv;
    XrgPaint2DColor(paint, stageColor);
    XrgPaint2DAlpha(paint, 0);
    XrgPaint2DDrawXYWH(paint, 0, 0, 0xDA, 0x200, 0x80);
    if (stageColor[3] >= 0x50)
    {
      _time_disp_title(pAnn, 0, 0xE8);
      _time_disp(pAnn, 0, 0x107, pAnn->stageClearTime);
      _damage_disp_title(pAnn, 0, 0x121);
      _damage_disp(pAnn, 0, 0x139, pAnn->stageClearDamage);
    }
  }
  if (pAnn->gameClearActive != 0)
  {
    XrgPaint2D *paint = pAnn->paint;
    int lim;
    int gtick;
    int gv;
    memset(gameColor, 0, 0x10);
    gtick = pAnn->stageClearTick;
    lim = 0x50;
    gameColor[3] = lim;
    totalTime = 0;
    gtick = gtick + 1;
    totalDamage = 0.0f;
    t = gtick * 8;
    pAnn->stageClearTick = gtick;
    gv = 0;
    if (t >= 0)
    {
      gv = _clamp_lim(lim, t);
    }
    gameColor[3] = gv;
    XrgPaint2DColor(paint, gameColor);
    XrgPaint2DAlpha(paint, 0);
    XrgPaint2DDrawXYWH(paint, 0, 0, 0xC8, 0x200, 0x96);
    if (gameColor[3] >= 0x50)
    {
      pal = D_00A586A0;
      i = 0;
      _time_disp_title(pAnn, -0x80, 0xCA);
      _damage_disp_title(pAnn, 0x80, 0xCE);
      if (pAnn->m_uStage != 0)
      {
        float *pd = pAnn->m_afTotalDamage;
        y = 0xE8;
        do
        {
          _time_disp(pAnn, -0x80, y, ((unsigned int *) pd)[-16]);
          new_var2 = pd;
          _damage_disp(pAnn, 0x80, y, *new_var2);
          i += 1;
          totalTime += ((unsigned int *) pd)[-16];
          totalDamage += *pd;
          y += 0x14;
          pd++;
        }
        while (i < pAnn->m_uStage);
      }
      XrgPaint2DAlpha(paint, 0);
      XrgPaint2DColor(paint, &pal);
      XrgPaint2DDrawXYWH(paint, 0, 0, 0x13C, 0x200, 1);
      _time_disp(pAnn, -0x80, 0x142, totalTime);
      _damage_disp(pAnn, 0x80, 0x142, totalDamage);
    }
  }
  _paint_flush(pAnn);
}

/*
 * The stack copy _time_disp_title and _damage_disp_title hand to _paint_uvwh.
 * Both titles move their rectangle a word pair at a time: uvOffset is the
 * u/v pair _paint_uvwh feeds to XrgPaint2DSetUVOffset, uvSize the w/h pair it
 * feeds to XrgPaint2DSetUVSize and to the draw call's own width/height.
 */
typedef struct RgAnnounceUvRectHalves {
    long long uvOffset;
    long long uvSize;
} RgAnnounceUvRectHalves;

/*
 * The two fixed title rectangles, asm-owned scaffold data (splat names, no
 * config/symbols/ov12.txt entry): ov12:0x00a586b0 belongs to the time title
 * and ov12:0x00a586c0 to the damage title.
 */
const RgAnnounceUvRectHalves D_00A586B0 = {
    0, 0x0000001800000200LL
};
const RgAnnounceUvRectHalves D_00A586C0 = {
    0x0000003800000000LL, 0x0000001200000200LL
};

static void _paint_uvwh(RgAnnounce *pAnn, int index, int x, int y,
                        const RgAnnounceUvRect *rect);

static void _time_disp_title(RgAnnounce *pAnn, int x, int y)
{
    RgAnnounceUvRectHalves rect;

    rect.uvSize = D_00A586B0.uvSize;
    rect.uvOffset = D_00A586B0.uvOffset;
    XrgPaint2DAlpha(pAnn->paint, XRG_PAINT2D_BLEND_ADD);
    _paint_uvwh(pAnn, 0x17, x, y, (const RgAnnounceUvRect *) &rect);
}

extern void *InstanceOfRgBattleCommonData(void);
extern int RgBattleCommonDataAnnounceFont(void *pEnv);
extern void RgFontStr(XrgPaint2D *paint, int fontId, const char *text, int x,
                      int y, int color);
static void _paint_add(RgAnnounce *pAnn, int index, int x, int y);

static void _time_disp(RgAnnounce *pAnn, int x, int y, unsigned int time)
{
    XrgPaint2D *paint;
    int fontId;
    unsigned int sec;
    char buf[16];

    paint = pAnn->paint;
    fontId = RgBattleCommonDataAnnounceFont(InstanceOfRgBattleCommonData());
    buf[0] = time / 6000 / 10 % 10 + '0';
    buf[1] = time / 6000 % 10 + '0';
    buf[2] = 0;
    ((int (*)())RgFontStr)(paint, fontId, buf, x + 0xE3, y, 0);
    sec = time % 6000;
    buf[0] = sec / 100 / 10 % 10 + '0';
    buf[1] = sec / 100 % 10 + '0';
    RgFontStr(paint, fontId, buf, x + 0x100, y, 0);
    sec %= 100;
    buf[0] = sec / 10 % 10 + '0';
    buf[1] = sec % 10 + '0';
    RgFontStr(paint, fontId, buf, x + 0x122, y, 0);
    _paint_add(pAnn, 0x18, x, y - 8);
}

static void _damage_disp_title(RgAnnounce *pAnn, int x, int y)
{
    RgAnnounceUvRectHalves rect;

    rect.uvSize = D_00A586C0.uvSize;
    rect.uvOffset = D_00A586C0.uvOffset;
    XrgPaint2DAlpha(pAnn->paint, XRG_PAINT2D_BLEND_ADD);
    _paint_uvwh(pAnn, 0x17, x, y, (const RgAnnounceUvRect *) &rect);
}

extern void *InstanceOfRgBattleCommonData(void);
extern int RgBattleCommonDataAnnounceFont(void *pEnv);
extern float floorf(float value);
extern void RgFontStr(XrgPaint2D *paint, int fontId, const char *text, int x,
                      int y, int color);
static void _paint_add(RgAnnounce *pAnn, int index, int x, int y);
/* ov12:0x00a586d0 is the scaffold-owned "%d" format string. */
const char D_00A586D0[] = "%d";

static void _damage_disp(RgAnnounce *pAnn, int x, int y, float damage)
{
    XrgPaint2D *paint;
    int fontId;
    unsigned int damageValue;
    char damageText[32];

    paint = pAnn->paint;
    fontId = RgBattleCommonDataAnnounceFont(InstanceOfRgBattleCommonData());
    damageValue = (unsigned int) floorf(damage);
    sprintf(damageText, D_00A586D0, damageValue);
    RgFontStr(paint, fontId, damageText, x + 0x11F, y, 1);
    _paint_add(pAnn, 0x19, x, y - 0x10);
}

static void _paint_add(RgAnnounce *pAnn, int index, int x, int y);
static void _paint_sub(RgAnnounce *pAnn, int index, int x, int y);

static void _round1(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x2B, 0, pAnn->dispY);
    _paint_add(pAnn, 0, 0, pAnn->dispY);
}

static void _round2(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x2C, 0, pAnn->dispY);
    _paint_add(pAnn, 1, 0, pAnn->dispY);
}

static void _round3(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x2D, 0, pAnn->dispY);
    _paint_add(pAnn, 2, 0, pAnn->dispY);
}

static void _ready(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x2A, 0, pAnn->dispY);
    _paint_add(pAnn, 3, 0, pAnn->dispY);
}

static void _fight(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x27, 0, pAnn->dispY);
    _paint_add(pAnn, 4, 0, pAnn->dispY);
}

static void _youwin(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x1B, 0, pAnn->dispY);
    _paint_add(pAnn, 5, 0, pAnn->dispY);
}

static void _youlose(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x30, 0, pAnn->dispY);
    _paint_add(pAnn, 6, 0, pAnn->dispY);
}

static void _draw(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x26, 0, pAnn->dispY);
    _paint_add(pAnn, 7, 0, pAnn->dispY);
}

static void _gameover(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x29, 0, pAnn->dispY);
    _paint_add(pAnn, 9, 0, pAnn->dispY);
}

static void _timeover(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x2F, 0, pAnn->dispY);
    _paint_add(pAnn, 8, 0, pAnn->dispY);
}

static void _stageclear(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x2E, 0, pAnn->dispY);
    _paint_add(pAnn, 0xA, 0, pAnn->dispY);
}

static void _gameclear(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x28, 0, pAnn->dispY);
    _paint_add(pAnn, 0xB, 0, pAnn->dispY);
}

static void _you_win_you_lose(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x1B, -RG_ANNOUNCE_COMBO_X_OFFSET, pAnn->dispY);
    _paint_add(pAnn, 5, -RG_ANNOUNCE_COMBO_X_OFFSET, pAnn->dispY);
    _paint_sub(pAnn, 0x30, RG_ANNOUNCE_COMBO_X_OFFSET, pAnn->dispY);
    _paint_add(pAnn, 6, RG_ANNOUNCE_COMBO_X_OFFSET, pAnn->dispY);
}

static void _you_lose_you_win(RgAnnounce *pAnn) {
    _paint_sub(pAnn, 0x1B, RG_ANNOUNCE_COMBO_X_OFFSET, pAnn->dispY);
    _paint_add(pAnn, 5, RG_ANNOUNCE_COMBO_X_OFFSET, pAnn->dispY);
    _paint_sub(pAnn, 0x30, -RG_ANNOUNCE_COMBO_X_OFFSET, pAnn->dispY);
    _paint_add(pAnn, 6, -RG_ANNOUNCE_COMBO_X_OFFSET, pAnn->dispY);
}

static void _paint(XrgPaint2D *paint, int texture, int blend, int x, int y)
{
    if (texture != 0) {
        XrgPaint2DUseTexture(paint, texture);
        XrgPaint2DAlpha(paint, blend);
        XrgPaint2DDrawXYWH(paint, XRG_PAINT2D_MODE_USE_PIC_SIZE, x, y, 0, 0);
    }
}

static void _paint_uvwh(RgAnnounce *pAnn, int index, int x, int y,
                        const RgAnnounceUvRect *rect)
{
    XrgPaint2D *paint = pAnn->paint;

    XrgPaint2DUseTexture(paint, pAnn->textures[index]);
    XrgPaint2DSetUVOffset(paint, rect->u, rect->v);
    XrgPaint2DSetUVSize(paint, rect->w, rect->h);
    XrgPaint2DDrawXYWH(paint, 0, x, y, rect->w, rect->h);
}

static void _paint_lin(RgAnnounce *pAnn, int index, int x, int y)
{
    _paint(pAnn->paint, pAnn->textures[index], XRG_PAINT2D_BLEND_LINEAR, x, y);
}

static void _paint_add(RgAnnounce *pAnn, int index, int x, int y)
{
    _paint(pAnn->paint, pAnn->textures[index], XRG_PAINT2D_BLEND_ADD, x, y);
}

static void _paint_sub(RgAnnounce *pAnn, int index, int x, int y)
{
    _paint(pAnn->paint, pAnn->textures[index], XRG_PAINT2D_BLEND_SUB, x, y);
}

static void _paint_flush(RgAnnounce *pAnn)
{
    XrgPaint2DFlush(pAnn->paint);
}

void RgAnnounceDispInit(RgAnnounce *pAnn, int kind)
{
    static RgAnnounceDrawFunc s_apFuncs[RG_ANNOUNCE_DRAW_KIND_COUNT] = {
        _round1, _round2, _round3, _ready, _fight, _youwin, _youlose,
        _draw, _gameover, _timeover, _stageclear, _gameclear,
        _you_win_you_lose, _you_lose_you_win
    };
    if (pAnn == 0) {
        assert_prog(D_00A57BB0, D_00A57BC0, 466);
    }
    if ((unsigned int) kind < RG_ANNOUNCE_DRAW_KIND_COUNT) {
        pAnn->timer = RG_ANNOUNCE_TIMER_INFINITE;
        pAnn->drawFunc = s_apFuncs[kind];
    } else {
        pAnn->drawFunc = 0;
    }
    pAnn->dispY = pAnn->pendingDispY;
    pAnn->pendingDispY = RG_ANNOUNCE_DISP_Y_DEFAULT;
}

void RgAnnounceDispInitTime(RgAnnounce *pAnn, int kind, float duration)
{
    RgAnnounceDispInit(pAnn, kind);
    if (pAnn->drawFunc != 0) {
        pAnn->timer = duration;
    }
}

void RgAnnounceDispOff(RgAnnounce *pAnn)
{
    if (pAnn == 0) {
        assert_prog(D_00A57BB0, D_00A57BC0, 493);
    }
    pAnn->drawFunc = 0;
    pAnn->stageClearActive = 0;
}

void RgAnnounceSetStageClear(RgAnnounce *pAnn, int time, float damage)
{
    if (pAnn == 0) {
        assert_prog(D_00A57BB0, D_00A57BC0, 505);
    }
    pAnn->stageClearTime = time;
    pAnn->stageClearDamage = damage;
    pAnn->gameClearActive = 0;
    pAnn->stageClearActive = 1;
    pAnn->stageClearTick = 0;
}

/* "uStage < sizeof(pAnn->m_auTotalTime) / sizeof(pAnn->m_auTotalTime[0])" */
const char D_00A586D8[] =
    "uStage < sizeof(pAnn->m_auTotalTime) / sizeof(pAnn->m_auTotalTime[0])";

void RgAnnounceSetGameClear(RgAnnounce *pAnn, const unsigned int *auTime,
                            const float *afDamage, unsigned int uStage)
{
    unsigned int i;

    if (pAnn == 0) {
        assert_prog(D_00A57BB0, D_00A57BC0, 521);
    }
    if (uStage >= RG_ANNOUNCE_GAMECLEAR_MAX) {
        assert_prog(D_00A586D8, D_00A57BC0, 522);
    }
    for (i = 0; i < uStage; i++) {
        pAnn->m_auTotalTime[i] = auTime[i];
        pAnn->m_afTotalDamage[i] = afDamage[i];
    }
    pAnn->m_uStage = uStage;
    pAnn->gameClearActive = 1;
    pAnn->stageClearActive = 0;
    pAnn->stageClearTick = 0;
}

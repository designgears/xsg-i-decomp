/*
 * OV12 original TU 8: 0x00a0d178..0x00a0db58 (5 functions)
 */
#include "common.h"
#include "shared.h"
#include "rg_robot_spec.h"

extern void assert_prog(const char *expression, const char *source_file, int line);
extern const char D_00A51E68[];
extern const char D_00A51E78[];
typedef struct RgSpecName { long long v; } RgSpecName;
extern const RgSpecName D_00A51E90[];
typedef struct RgRobotSpecFull {
    int id;                 /* +0x00 */
    int type;               /* +0x04 */
    int unk08;              /* +0x08 */
    float f0C;
    float f10;
    float f14;
    float f18;
    float f1C;
    float f20;
    float f24;
    float f28;
    float f2C;
    float f30;
    float f34;
    float f38;
    float f3C;
    int i40;
    float f44;
    int i48;
    float f4C;
    float f50;
    float f54;
    char s58[0x20];
    char s78[0x20];
    RgSpecName name;        /* +0x98 */
} RgRobotSpecFull;

void InitRgRobotSpec(RgRobotSpecFull *pSpec)
{
    if (pSpec == 0) {
        assert_prog(D_00A51E68, D_00A51E78, 0x16);
    }
    pSpec->name = D_00A51E90[0];
    pSpec->id = -1;
    pSpec->f0C = 20.0f;
    pSpec->f10 = 40.0f;
    pSpec->f14 = 0.35298797f;
    pSpec->f18 = 0.1f;
    pSpec->f1C = 1000.0f;
    pSpec->f20 = 60000.0f;
    pSpec->f24 = 62831.855f;
    pSpec->f28 = 2.8f;
    pSpec->f2C = 9.0f;
    pSpec->f30 = 0.3f;
    pSpec->f34 = 3.0f;
    pSpec->f38 = 0.2f;
    pSpec->f3C = 0.03f;
    pSpec->i40 = 3;
    pSpec->f44 = 0.34f;
    pSpec->i48 = 6;
    pSpec->f4C = 100.0f;
    pSpec->f50 = 0.04f;
    pSpec->f54 = 10.0f;
    pSpec->s58[0] = 0;
    pSpec->s78[0] = 0;
    pSpec->type = 0;
}

static void _ResetAnaAutoHoming(void)
{
    s_nAnaAutoHomingID = -1;
}

extern double atof(const char *);
extern int strcmp(const char *, const char *);
extern void RgError(const char *message, const char *source_file, int line, ...);
extern const char D_00A51E98[];
extern const char D_00A51EE0[];
extern const char D_00A51EF0[];
extern const char D_00A51EF8[];
extern const char D_00A51F00[];
extern const char D_00A51F08[];
typedef struct RgAutoHoming {
    float angle;
    unsigned int flags;
} RgAutoHoming;
typedef struct RgRobotParam {
    char pad[0x30];
    RgAutoHoming m_aAutoHoming[2];
} RgRobotParam;

static void _AnaAutoHoming(RgRobotSpecFull *pSpec, char *str, int bValue)
{
    unsigned int nID;

    if (bValue == 0) {
        nID = ++s_nAnaAutoHomingID;
        if (nID >= 2) {
            assert_prog(D_00A51E98, D_00A51E78, 0x4D);
        }
        ((RgRobotParam *)&pSpec->f0C)->m_aAutoHoming[nID].angle = atof(str) * 3.1415927f;
        ((RgRobotParam *)&pSpec->f0C)->m_aAutoHoming[nID].flags = 0;
        return;
    }
    nID = s_nAnaAutoHomingID;
    if (nID >= 2) {
        assert_prog(D_00A51E98, D_00A51E78, 0x53);
    }
    if (strcmp(str, D_00A51EE0) == 0) {
        ((RgRobotParam *)&pSpec->f0C)->m_aAutoHoming[nID].flags |= 1;
    } else if (strcmp(str, D_00A51EF0) == 0) {
        ((RgRobotParam *)&pSpec->f0C)->m_aAutoHoming[nID].flags |= 2;
    } else if (strcmp(str, D_00A51EF8) == 0) {
        ((RgRobotParam *)&pSpec->f0C)->m_aAutoHoming[nID].flags |= 4;
    } else if (strcmp(str, D_00A51F00) == 0) {
        ((RgRobotParam *)&pSpec->f0C)->m_aAutoHoming[nID].flags |= 8;
    } else {
        RgError(D_00A51F08, D_00A51E78, 0x5F, str);
    }
}

INCLUDE_ASM("asm/nonmatchings/ov12/rg_robot_spec", RgRobotSpecReadFromText);

extern void XrgLog(const char *format, const char *source_file, int line, ...);
extern const char D_00A520A0[];
extern const char D_00A520C0[];
extern const char D_00A520E0[];
extern const char D_00A52100[];
extern const char D_00A52120[];
extern const char D_00A52140[];
extern const char D_00A52160[];
extern const char D_00A52180[];
extern const char D_00A521A0[];
extern const char D_00A521C0[];
extern const char D_00A521E0[];
extern const char D_00A52200[];
extern const char D_00A52220[];
extern const char D_00A52240[];
extern const char D_00A52260[];

void RgRobotSpecDump(RgRobotSpecFull *pSpec)
{
    if (pSpec == 0) {
        assert_prog(D_00A51E68, D_00A51E78, 0x9E);
    }
    XrgLog(D_00A520A0, D_00A51E78, 0xA0, pSpec->f20);
    XrgLog(D_00A520C0, D_00A51E78, 0xA1, pSpec->f24);
    XrgLog(D_00A520E0, D_00A51E78, 0xA2, pSpec->f0C);
    XrgLog(D_00A52100, D_00A51E78, 0xA3, pSpec->f14);
    XrgLog(D_00A52120, D_00A51E78, 0xA4, pSpec->f18);
    XrgLog(D_00A52140, D_00A51E78, 0xA5, pSpec->f1C);
    XrgLog(D_00A52160, D_00A51E78, 0xA6, pSpec->f28);
    XrgLog(D_00A52180, D_00A51E78, 0xA7, pSpec->f2C);
    XrgLog(D_00A521A0, D_00A51E78, 0xA8, pSpec->f30);
    XrgLog(D_00A521C0, D_00A51E78, 0xA9, pSpec->f34);
    XrgLog(D_00A521E0, D_00A51E78, 0xAA, pSpec->f38);
    XrgLog(D_00A52200, D_00A51E78, 0xAC, pSpec->f3C, pSpec->i40);
    XrgLog(D_00A52220, D_00A51E78, 0xAD, pSpec->f44, pSpec->i48);
    XrgLog(D_00A52240, D_00A51E78, 0xAE, pSpec->f4C);
    XrgLog(D_00A52260, D_00A51E78, 0xAF, pSpec->f50);
}

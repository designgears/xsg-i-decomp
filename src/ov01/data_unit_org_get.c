/*
 * OV01 original TU 5: 0x00a191c0..0x00a1d348 (93 functions)
 */
#include "common.h"
#include "data_unit_org_get.h"
#include "ov01/calc.h"

/* Types recovered upstream (origin/master); kept for reference. */
typedef struct BattleDataHeader {
    unsigned char unmodeled_00[8];
    int unitInitOffset; /* +0x08 */
    int techniqueOffset;
    int normalInitOffset; /* +0x10 */
    int specialInitOffset; /* +0x14 */
    int etherInitOffset; /* +0x18 */
    int weaponOffset;
    int ethOffset;
    int itmOffset;
    int attOffset;
    int accessoryOffset;
    int skillOffset;
    int gainOffset;
    int experienceOffset; /* +0x38 */
    int parameterOffset; /* +0x3C */
    int specialTableOffset; /* +0x40 */
    int defaultEquipmentOffset; /* +0x44 */
} BattleDataHeader;

/* Fixed-size records the data*Get accessors index (upstream recovery). */
typedef struct NormalInitRecord { unsigned char opaque[0x0C]; } NormalInitRecord;
typedef struct SpecialInitRecord { unsigned char opaque[0x0C]; } SpecialInitRecord;
typedef struct EtherInitRecord { unsigned char opaque[0x18]; } EtherInitRecord;
typedef struct ParameterRecord { unsigned char opaque[0x20]; } ParameterRecord;
typedef struct DefaultEquipmentRecord { unsigned char opaque[0x20]; } DefaultEquipmentRecord;
typedef struct SpecialTableRecord { unsigned char opaque[0x20]; } SpecialTableRecord;


typedef struct XtxFileEntry {
    const char *name;
    unsigned int address;
} XtxFileEntry;

typedef struct PosTblEntry {
    float position[4];
    short line; /* +0x10 */
    unsigned char unmodeled_12[0x20 - 0x12];
} PosTblEntry;

typedef struct SpecTable {
    int specialBase[0x3C / sizeof(int)];
    int bakpBase[1]; /* +0x3C, indexed dynamically */
} SpecTable;

/* ---- initialized data (original order) ---- */
const char D_00A44880[] = "data\\yamamoto\\mot\\";
const char D_00A44898[] = ".bin";
const char D_00A448A0[] = "data\\yamamoto\\map\\";
const char D_00A448B8[] = "map";
const char D_00A448C0[] = "data\\yamamoto\\think\\";
const char D_00A448D8[] = "think";
const char D_00A448E0[] = "data\\yamamoto\\stat\\cur.xtx";
const char D_00A44900[] = "data\\yamamoto\\stat\\manu.xtx";
const char D_00A44920[] = "data\\yamamoto\\stat\\stat.xtx";
const char D_00A44940[] = "def0";
const char D_00A44948[] = "sol0";
const char D_00A44950[] = "wir0";
const char D_00A44958[] = "smp1";
const char D_00A44960[] = "ecm2";
const char D_00A44968[] = "ecm1";
const char D_00A44970[] = "ead0";
const char D_00A44978[] = "bbc0";
const char D_00A44980[] = "vsd0";
const char D_00A44988[] = "fsd0";
const char D_00A44990[] = "shb0";
const char D_00A44998[] = "amp2";
const char D_00A449A0[] = "lgc0";
const char D_00A449A8[] = "bp0";
const char D_00A449B0[] = "eac0";
const char D_00A449B8[] = "cb0";
const char D_00A449C0[] = "bl0";
const char D_00A449C8[] = "msp2";
const char D_00A449D0[] = "msp0";
const char D_00A449D8[] = "hb0";
const char D_00A449E0[] = "glg0";
const char D_00A449E8[] = "gnd0";
const char D_00A449F0[] = "fl0";
const char D_00A449F8[] = "pbk0";
const char D_00A44A00[] = "lsr0";
const char D_00A44A08[] = "bs0";
const char D_00A44A10[] = "bam0";
const char D_00A44A18[] = "rf2";
const char D_00A44A20[] = "rf1";
const char D_00A44A28[] = "rf0";
const char D_00A44A30[] = "mg1";
const char D_00A44A38[] = "mg0";
const char D_00A44A40[] = "hmr2";
const char D_00A44A48[] = "hmr1";
const char D_00A44A50[] = "hmr0";
const char D_00A44A58[] = "hg1";
const char D_00A44A60[] = "hg0";
const char D_00A44A68[] = "bdf0";
const char D_00A44A70[] = "bd1";
const char D_00A44A78[] = "bd0";
const char D_00A44A80[] = "ax0";
const char D_00A44A88[] = "sut0";
const char D_00A44A90[] = "zigg_wp4";
const char D_00A44AA0[] = "zigg_wp3";
const char D_00A44AB0[] = "zigg_wp2";
const char D_00A44AC0[] = "zigg_wp1";
const char D_00A44AD0[] = "kosm_wp6";
const char D_00A44AE0[] = "kosm_wp5";
const char D_00A44AF0[] = "kosm_wp4";
const char D_00A44B00[] = "kosm_wp3";
const char D_00A44B10[] = "kosm_wp2";
const char D_00A44B20[] = "kosm_wp1";
const char D_00A44B30[] = "jr_sp8";
const char D_00A44B38[] = "jr_sp7";
const char D_00A44B40[] = "jr_sp6";
const char D_00A44B48[] = "jr_sp5";
const char D_00A44B50[] = "jr_sp4";
const char D_00A44B58[] = "jr_sp3";
const char D_00A44B60[] = "jr_sp2";
const char D_00A44B68[] = "jr_sp1";
const char D_00A44B70[] = "momo_sp8";
const char D_00A44B80[] = "momo_sp7";
const char D_00A44B90[] = "momo_sp6";
const char D_00A44BA0[] = "momo_sp5";
const char D_00A44BB0[] = "momo_sp4";
const char D_00A44BC0[] = "momo_sp3";
const char D_00A44BD0[] = "momo_sp2";
const char D_00A44BE0[] = "momo_sp1";
const char D_00A44BF0[] = "zigg_sp7";
const char D_00A44C00[] = "zigg_sp6";
const char D_00A44C10[] = "zigg_sp5";
const char D_00A44C20[] = "zigg_sp4";
const char D_00A44C30[] = "zigg_sp3";
const char D_00A44C40[] = "zigg_sp2";
const char D_00A44C50[] = "zigg_sp1";
const char D_00A44C60[] = "shit_sp8";
const char D_00A44C70[] = "shit_sp7";
const char D_00A44C80[] = "shit_sp6";
const char D_00A44C90[] = "shit_sp5";
const char D_00A44CA0[] = "shit_sp4";
const char D_00A44CB0[] = "shit_sp3";
const char D_00A44CC0[] = "shit_sp2";
const char D_00A44CD0[] = "shit_sp1";
const char D_00A44CE0[] = "shio_sp8";
const char D_00A44CF0[] = "shio_sp7";
const char D_00A44D00[] = "shio_sp6";
const char D_00A44D10[] = "shio_sp5";
const char D_00A44D20[] = "shio_sp4";
const char D_00A44D30[] = "shio_sp3";
const char D_00A44D40[] = "shio_sp2";
const char D_00A44D50[] = "shio_sp1";
const char D_00A44D60[] = "kosm_sp7";
const char D_00A44D70[] = "kosm_sp6";
const char D_00A44D80[] = "kosm_sp5";
const char D_00A44D90[] = "kosm_sp4";
const char D_00A44DA0[] = "kosm_sp3";
const char D_00A44DB0[] = "kosm_sp2";
const char D_00A44DC0[] = "kosm_sp1";
const char D_00A44DD0[] = "chao_sp8";
const char D_00A44DE0[] = "chao_sp7";
const char D_00A44DF0[] = "chao_sp6";
const char D_00A44E00[] = "chao_sp5";
const char D_00A44E10[] = "chao_sp4";
const char D_00A44E20[] = "chao_sp3";
const char D_00A44E30[] = "chao_sp2";
const char D_00A44E40[] = "chao_sp1";
const char D_00A44E50[] = "skj_kosmos1";
const char D_00A44E60[] = "skj_shion4";
const char D_00A44E70[] = "skj_shion3";
const char D_00A44E80[] = "skj_shion2";
const char D_00A44E90[] = "skj_shion1";
const char D_00A44EA0[] = "skj_momo1";
const char D_00A44EB0[] = "skj_jr2";
const char D_00A44EB8[] = "skj_jr1";
const char D_00A44EC0[] = "boss_033";
const char D_00A44ED0[] = "boss_032";
const char D_00A44EE0[] = "boss_031";
const char D_00A44EF0[] = "boss_030";
const char D_00A44F00[] = "boss_029";
const char D_00A44F10[] = "boss_028";
const char D_00A44F20[] = "boss_027";
const char D_00A44F30[] = "boss_026";
const char D_00A44F40[] = "boss_025";
const char D_00A44F50[] = "boss_024";
const char D_00A44F60[] = "boss_023";
const char D_00A44F70[] = "boss_022";
const char D_00A44F80[] = "boss_021";
const char D_00A44F90[] = "boss_020";
const char D_00A44FA0[] = "boss_019";
const char D_00A44FB0[] = "boss_018";
const char D_00A44FC0[] = "boss_017";
const char D_00A44FD0[] = "boss_016";
const char D_00A44FE0[] = "boss_015";
const char D_00A44FF0[] = "boss_014";
const char D_00A45000[] = "boss_013";
const char D_00A45010[] = "boss_012";
const char D_00A45020[] = "boss_011";
const char D_00A45030[] = "boss_010";
const char D_00A45040[] = "boss_009";
const char D_00A45050[] = "boss_008";
const char D_00A45060[] = "boss_007";
const char D_00A45070[] = "boss_006";
const char D_00A45080[] = "boss_005";
const char D_00A45090[] = "boss_004";
const char D_00A450A0[] = "boss_003";
const char D_00A450B0[] = "boss_002";
const char D_00A450C0[] = "boss_001";
const char D_00A450D0[] = "utso_007";
const char D_00A450E0[] = "utso_006";
const char D_00A450F0[] = "utso_005";
const char D_00A45100[] = "utso_004";
const char D_00A45110[] = "utso_003";
const char D_00A45120[] = "utso_002";
const char D_00A45130[] = "utso_001";
const char D_00A45140[] = "utro_007";
const char D_00A45150[] = "utro_006";
const char D_00A45160[] = "utro_005";
const char D_00A45170[] = "utro_004";
const char D_00A45180[] = "utro_003";
const char D_00A45190[] = "utro_002";
const char D_00A451A0[] = "utro_001";
const char D_00A451B0[] = "utre_006";
const char D_00A451C0[] = "utre_005";
const char D_00A451D0[] = "utre_004";
const char D_00A451E0[] = "utre_003";
const char D_00A451F0[] = "utre_002";
const char D_00A45200[] = "utre_001";
const char D_00A45210[] = "utma_014";
const char D_00A45220[] = "utma_013";
const char D_00A45230[] = "utma_012";
const char D_00A45240[] = "utma_011";
const char D_00A45250[] = "utma_010";
const char D_00A45260[] = "utma_009";
const char D_00A45270[] = "utma_008";
const char D_00A45280[] = "utma_007";
const char D_00A45290[] = "utma_006";
const char D_00A452A0[] = "utma_005";
const char D_00A452B0[] = "utma_004";
const char D_00A452C0[] = "utma_003";
const char D_00A452D0[] = "utma_002";
const char D_00A452E0[] = "utma_001";
const char D_00A452F0[] = "feso_004";
const char D_00A45300[] = "feso_003";
const char D_00A45310[] = "feso_002";
const char D_00A45320[] = "feso_001";
const char D_00A45330[] = "fero_001";
const char D_00A45340[] = "fere_003";
const char D_00A45350[] = "fere_002";
const char D_00A45360[] = "fere_001";
const char D_00A45370[] = "fema_006";
const char D_00A45380[] = "fema_005";
const char D_00A45390[] = "fema_004";
const char D_00A453A0[] = "fema_003";
const char D_00A453B0[] = "fema_002";
const char D_00A453C0[] = "fema_001";
const char D_00A453D0[] = "guno_114";
const char D_00A453E0[] = "guno_025";
const char D_00A453F0[] = "guno_024";
const char D_00A45400[] = "guno_023";
const char D_00A45410[] = "guno_022";
const char D_00A45420[] = "guno_021";
const char D_00A45430[] = "guno_020";
const char D_00A45440[] = "guno_019";
const char D_00A45450[] = "guno_018";
const char D_00A45460[] = "guno_017";
const char D_00A45470[] = "guno_016";
const char D_00A45480[] = "guno_015";
const char D_00A45490[] = "guno_014";
const char D_00A454A0[] = "guno_013";
const char D_00A454B0[] = "guno_012";
const char D_00A454C0[] = "guno_011";
const char D_00A454D0[] = "guno_010";
const char D_00A454E0[] = "guno_009";
const char D_00A454F0[] = "guno_008";
const char D_00A45500[] = "guno_007";
const char D_00A45510[] = "guno_006";
const char D_00A45520[] = "guno_005";
const char D_00A45530[] = "guno_004";
const char D_00A45540[] = "guno_003";
const char D_00A45550[] = "guno_002";
const char D_00A45560[] = "guno_001";
const char D_00A45570[] = "sld";
const char D_00A45578[] = "ukn";
const char D_00A45580[] = "ag05";
const char D_00A45588[] = "ag04";
const char D_00A45590[] = "ag03";
const char D_00A45598[] = "ag02";
const char D_00A455A0[] = "ag01";
const char D_00A455A8[] = "vx06";
const char D_00A455B0[] = "vx05";
const char D_00A455B8[] = "vx04";
const char D_00A455C0[] = "vx03";
const char D_00A455C8[] = "vx02";
const char D_00A455D0[] = "vx01";
const char D_00A455D8[] = "virg";
const char D_00A455E0[] = "cath";
const char D_00A455E8[] = "ceci";
const char D_00A455F0[] = "jr";
const char D_00A455F8[] = "momo";
const char D_00A45600[] = "zigg";
const char D_00A45608[] = "shit";
const char D_00A45610[] = "shio";
const char D_00A45618[] = "kosm";
const char D_00A45620[] = "chao";
const char D_00A45628[] = "uk_";
const char D_00A45630[] = "a6_";
const char D_00A45638[] = "a5_";
const char D_00A45640[] = "a4_";
const char D_00A45648[] = "a3_";
const char D_00A45650[] = "a2_";
const char D_00A45658[] = "a1_";
const char D_00A45660[] = "v6_";
const char D_00A45668[] = "v5_";
const char D_00A45670[] = "v4_";
const char D_00A45678[] = "v3_";
const char D_00A45680[] = "v2_";
const char D_00A45688[] = "v1_";
const char D_00A45690[] = "d";
const char D_00A45698[] = "l";
const char D_00A456A0[] = "r";
char mdlFileName[256] = { 0 };
char mdlFileName2[256] = { 0 };
char motFileName[256] = { 0 };
char *motNameBase = D_00A44880;
char *motNameExt = D_00A44898;
char *mapNameBase = D_00A448A0;
char *mapNameExt = D_00A44898;
char *mapName = D_00A448B8;
char *thinkNameBase = D_00A448C0;
char *thinkNameExt = D_00A44898;
char *thinkName = D_00A448D8;
XtxFileEntry xtxFile[4] = { D_00A44920, 0x1B13800, D_00A44900, 0x1B09800, D_00A448E0, 0x1B04000, 0, 0 };
unsigned short xtxAdr[0x13] = { 0x3800, 0x3AE0, 0x3908, 0x3AE4, 0x38C0, 0x3AF4, 0x38D0, 0x3AE4, 0x3840, 0x3AF0, 0x3AD4, 0x3880, 0x3AD0, 0x3800, 0x3840, 0x3800, 0x3820, 0x3840, 0x3828 };
PosTblEntry posTbl[36] __attribute__((aligned(16))) = { 0.0f, 0.0f, 0.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, 2.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, 0.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, -2.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4.0f, 0.0f, 2.0f, 1.0f, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4.0f, 0.0f, 0.0f, 1.0f, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4.0f, 0.0f, -2.0f, 1.0f, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5.0f, 0.0f, -4.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6.0f, 0.0f, 0.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5.0f, 0.0f, 4.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, -3.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, -2.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, -1.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, 0.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, 1.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, 2.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0f, 0.0f, 3.0f, 1.0f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3.0f, 0.0f, -4.0f, 1.0f, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3.0f, 0.0f, 4.0f, 1.0f, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4.0f, 0.0f, -2.0f, 1.0f, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4.0f, 0.0f, 0.0f, 1.0f, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4.0f, 0.0f, 2.0f, 1.0f, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5.0f, 0.0f, -5.0f, 1.0f, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5.0f, 0.0f, 5.0f, 1.0f, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6.0f, 0.0f, 0.0f, 1.0f, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8.0f, 0.0f, -3.0f, 1.0f, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8.0f, 0.0f, 0.0f, 1.0f, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8.0f, 0.0f, 3.0f, 1.0f, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14.0f, 0.0f, 0.0f, 1.0f, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7.0f, 0.0f, -5.0f, 1.0f, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7.0f, 0.0f, 5.0f, 1.0f, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13.0f, 0.0f, -9.0f, 1.0f, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13.0f, 0.0f, 9.0f, 1.0f, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9.0f, 0.0f, -9.0f, 1.0f, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9.0f, 0.0f, 9.0f, 1.0f, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18.0f, 0.0f, 0.0f, 1.0f, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
short widChgTbl[116] = { 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3801, 0x3001, 0x3002, 0x3003, 0x3004, 0x3005, 0x3006, 0x3007, 0x3000, 0x3008, 0x3009, 0x300A, 0x3000, 0x3101, 0x3102, 0x3103, 0x3104, 0x3105, 0x3106, 0x3107, 0x3108, 0x3109, 0x310A, 0x310B, 0x310C, 0x310D, 0x310E, 0x310F, 0x3110, 0x3111, 0x3201, 0x3202, 0x3203, 0x3204, 0x3205, 0x3301, 0x3302, 0x3303, 0x3304, 0x3305, 0x3306, 0x3307, 0x3308, 0x3000, 0x3000, 0x3000, 0x3401, 0x3402, 0x3403, 0x3404, 0x3405, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3501, 0x3000, 0x3601, 0x3602, 0x3603, 0x3608, 0x3605, 0x3606, 0x3607, 0x3604, 0x3609, 0x360A, 0x360B, 0x360C, 0x360D, 0x360E, 0x360F, 0x3610, 0x3613, 0x3614, 0x3615, 0x3616, 0x3617, 0x3618, 0x3619, 0x361A, 0x361B, 0x361C, 0x361D, 0x361E, 0x361F, 0x3620, 0x3623, 0x3624, 0x3625, 0x3626, 0x3628, 0x3629, 0x362A, 0x362B, 0x362C, 0x362D, 0x362E, 0x362F, 0x3630, 0x3631, 0x3701 };
short cidChgTbl[196] = { 0, 3, 2, 1, 7, 6, 4, 5, 0, 0, 0x110, 0, 0, 0, 0, 0, 0, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2101, 0x2102, 0x2103, 0x2104, 0x2105, 0, 0x2201, 0, 0, 0, 0x20E, 0x4001, 0x4002, 0x4003, 0x4004, 0x4005, 0x4006, 0x4007, 0x4008, 0x4009, 0x400A, 0x400B, 0x400C, 0x400D, 0x400E, 0x400F, 0x4010, 0x4011, 0x4012, 0x4013, 0x4014, 0x4015, 0x4016, 0x4017, 0x4018, 0x4019, 0x4001, 0x4002, 0x4003, 0x4037, 0x4501, 0x4502, 0x4503, 0x4504, 0x4505, 0x4506, 0x4801, 0x4802, 0x4803, 0x4601, 0x4701, 0x4702, 0x4703, 0x4704, 0x4101, 0x4102, 0x4103, 0x4104, 0x4105, 0x4106, 0x4107, 0x4108, 0x4109, 0x410A, 0x410B, 0x410C, 0x410D, 0x410E, 0x4201, 0x4202, 0x4203, 0x4204, 0x4205, 0x4206, 0x4401, 0x4402, 0x4403, 0x4404, 0x4405, 0x4406, 0x4407, 0x4301, 0x4302, 0x4303, 0x4304, 0x4305, 0x4306, 0x4307, 0x4102, 0x4102, 0x4106, 0x4006, 0x4501, 0x4701, 0x4702, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4701, 0x4701, 0x4701, 0x4701, 0x4701, 0x4F01, 0x4F02, 0x4F03, 0x4F04, 0x4F05, 0x4F06, 0x4F07, 0x4F08, 0x4F09, 0x4F0A, 0x4F0B, 0x4F0C, 0x4F0D, 0x4F0E, 0x4F0F, 0x4F10, 0x4F11, 0x4F12, 0x4F13, 0x4F14, 0x4F15, 0x4F16, 0x4F17, 0x4F18, 0x4F19, 0x4F1A, 0x4F1B, 0x4F1C, 0x4F1D, 0x4F1E, 0x4F1F, 0x4F20, 0x4F21, 0x4F03, 0x4F04, 0x4F14, 0x4F22, 0x4F23, 0x4F24, 0x4F25, 0x4F26, 0x4F27, 0x4F28, 0x4F29, 0 };
char *motFile[303] = { D_00A45620, D_00A45618, D_00A45610, D_00A45608, D_00A45600, D_00A455F8, D_00A455F0, D_00A455E8, D_00A455E0, D_00A455D8, D_00A45620, D_00A45620, 0, 0, 0, 0, D_00A455D0, D_00A455C8, D_00A455C0, D_00A455B8, D_00A455B0, D_00A455A8, D_00A455A0, D_00A45598, D_00A45590, D_00A45588, D_00A45580, D_00A45580, D_00A45578, 0, 0, 0, D_00A45570, D_00A45560, D_00A45550, D_00A45540, D_00A45530, D_00A45520, D_00A45510, D_00A45500, D_00A454F0, D_00A454E0, D_00A454D0, D_00A454C0, D_00A454B0, D_00A454A0, D_00A45490, D_00A45480, D_00A45470, D_00A45460, D_00A45450, D_00A45440, D_00A45430, D_00A45420, D_00A45410, D_00A45400, D_00A453F0, D_00A453E0, D_00A45560, D_00A45550, D_00A45540, D_00A453D0, D_00A453C0, D_00A453B0, D_00A453A0, D_00A45390, D_00A45380, D_00A45370, D_00A45360, D_00A45350, D_00A45340, D_00A45330, D_00A45320, D_00A45310, D_00A45300, D_00A452F0, D_00A452E0, D_00A452D0, D_00A452C0, D_00A452B0, D_00A452A0, D_00A45290, D_00A45280, D_00A45270, D_00A45260, D_00A45250, D_00A45240, D_00A45230, D_00A45220, D_00A45210, D_00A45200, D_00A451F0, D_00A451E0, D_00A451D0, D_00A451C0, D_00A451B0, D_00A451A0, D_00A45190, D_00A45180, D_00A45170, D_00A45160, D_00A45150, D_00A45140, D_00A45130, D_00A45120, D_00A45110, D_00A45100, D_00A450F0, D_00A450E0, D_00A450D0, D_00A452D0, D_00A452D0, D_00A45290, D_00A45510, D_00A453C0, D_00A45320, D_00A45310, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, D_00A45320, D_00A45320, D_00A45320, D_00A45320, D_00A45320, D_00A450C0, D_00A450B0, D_00A450A0, D_00A45090, D_00A45080, D_00A45070, D_00A45060, D_00A45050, D_00A45040, D_00A45030, D_00A45020, D_00A45010, D_00A45000, D_00A44FF0, D_00A44FE0, D_00A44FD0, D_00A44FC0, D_00A44FB0, D_00A44FA0, D_00A44F90, D_00A44F80, D_00A44F70, D_00A44F60, D_00A44F50, D_00A44F40, D_00A44F30, D_00A44F20, D_00A44F10, D_00A44F00, D_00A44EF0, D_00A44EE0, D_00A44ED0, D_00A44EC0, D_00A450A0, D_00A45090, D_00A44F90, D_00A44EB8, D_00A44EB0, D_00A44EA0, D_00A44E90, D_00A44E80, D_00A44E70, D_00A44E60, D_00A44E50, D_00A44E40, D_00A44E30, D_00A44E20, D_00A44E10, D_00A44E00, D_00A44DF0, D_00A44DE0, D_00A44DD0, D_00A44DC0, D_00A44DB0, D_00A44DA0, D_00A44D90, D_00A44D80, D_00A44D70, D_00A44D60, D_00A44D50, D_00A44D40, D_00A44D30, D_00A44D20, D_00A44D10, D_00A44D00, D_00A44CF0, D_00A44CE0, D_00A44CD0, D_00A44CC0, D_00A44CB0, D_00A44CA0, D_00A44C90, D_00A44C80, D_00A44C70, D_00A44C60, D_00A44C50, D_00A44C40, D_00A44C30, D_00A44C20, D_00A44C10, D_00A44C00, D_00A44BF0, D_00A44BE0, D_00A44BD0, D_00A44BC0, D_00A44BB0, D_00A44BA0, D_00A44B90, D_00A44B80, D_00A44B70, D_00A44B68, D_00A44B60, D_00A44B58, D_00A44B50, D_00A44B48, D_00A44B40, D_00A44B38, D_00A44B30, D_00A44B20, D_00A44B10, D_00A44B00, D_00A44AF0, D_00A44AE0, D_00A44AD0, D_00A44AC0, D_00A44AB0, D_00A44AA0, D_00A44A90, D_00A44A88, D_00A44A80, D_00A44A78, D_00A44A70, D_00A44A68, D_00A44A60, D_00A44A58, D_00A44A50, D_00A44A48, D_00A44A40, D_00A44A38, D_00A44A30, D_00A44A28, D_00A44A20, D_00A44A18, D_00A44A10, D_00A44A08, D_00A44A00, D_00A449F8, D_00A449F0, D_00A449E8, D_00A449E0, D_00A449D8, D_00A449D0, D_00A449C8, D_00A449C0, D_00A449B8, D_00A449B0, D_00A449A8, D_00A449A0, D_00A44998, D_00A44990, D_00A44988, D_00A44980, D_00A44978, D_00A44970, D_00A44968, D_00A44960, D_00A44958, D_00A44950, D_00A44948, D_00A44940, D_00A449E0, D_00A449D8, D_00A449C8 };
char *agwsName[13] = { D_00A45688, D_00A45680, D_00A45678, D_00A45670, D_00A45668, D_00A45660, D_00A45658, D_00A45650, D_00A45648, D_00A45640, D_00A45638, D_00A45630, D_00A45628 };
char *agwsLRName[3] = { D_00A456A0, D_00A45698, D_00A45690 };
int thinkMapTbl[100] = { 0, 0x24, 0x26, 0x27, 0x28, 0x29, 0x2F, 0x30, 0x32, 0x36, 0x39, 0x44C, 0x44D, 0x44F, 0x450, 0x451, 0x75, 0x76, 0x77, 0x7A, 0x7B, 0x5DE, 0x5DF, 0x5E0, 0x5E1, 0x5E4, 0x6A4, 0x6A5, 0x6A6, 0x6A7, 0x6A8, 0x6A9, 0x6AA, 0x6AB, 0x6AD, 0x6AE, 0x6AF, 0x4B2, 0x4B3, 0x4B7, 0x4BB, 0x4BC, 0x4BD, 0x4BE, 0x4BF, 0x4C0, 0x4C2, 0x4C3, 0x4CE, 0x523, 0x524, 0x525, 0x526, 0x527, 0x528, 0x529, 0x52A, 0x384, 0x385, 0x386, 0x387, 0x388, 0x389, 0x38A, 0x38B, 0x38C, 0x38D, 0x390, 0x324, 0x329, 0x32B, 0x32C, 0x32D, 0x32E, 0x32F, 0x579, 0x57B, 0x580, 0x581, 0x582, 0x583, 0x584, 0x585, 0x589, 0x58A, 0x58B, 0x58D, 0x58F, 0x331, 0x517, 0x37, 0x327, 0, 0, 0, 0, 0, 0, 0, 0 };
SpecTable specTbl = { 0x2D, 0x35, 0x3C, 0x44, 0x4C, 0x53, 0x5B, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int bakpTbl[16] = { 0, 0x2ED, 0, 0, 0x305, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char *mdlAdrTbl[7] = { (void *)0x1000000, (void *)0x1180000, (void *)0x1300000, (void *)0x1480000, (void *)0x1580000, (void *)0x1680000, (void *)0x1780000 };
void *motAdrTbl[7] = { (void *)0x179E000, (void *)0x17DE000, (void *)0x181E000, (void *)0x185E000, (void *)0x1890000, (void *)0x18C2000, (void *)0x18F4000 };

/*
 * orgData is a 33-entry table of 0x180-byte unit-origin records; callers
 * pass a one-based index, so dataUnitOrgGet returns orgData[id - 1].
 */
typedef struct UnitOrgRecord {
    unsigned char unmodeled_0[0x180];
} UnitOrgRecord;

const char D_00A456A8[] = "** dataUnitOrgGet: err %d\n";
extern UnitOrgRecord orgData[];

void *dataUnitOrgGet(int unitOrgId)
{
    if (unitOrgId >= 0x21) {
        printf(D_00A456A8, unitOrgId);
        return 0;
    }
    return &orgData[unitOrgId - 1];
}

const char D_00A456C8[] = "** dataPlChaGet: err %d\n";
extern unsigned char plChaData[];

PlCharacter *dataPlChaGet(int cid)
{
    unsigned char *base;

    if (cid >= 0x21) {
        printf(D_00A456C8, cid);
        return 0;
    }
    base = plChaData - 0xA8;
    return (PlCharacter *)(base + cid * 0xA8);
}

/*
 * D_426F48, D_426F50, D_426F54 and D_426F58 are byte offsets, from their own
 * loaded battle-data table's start (8, 0x10, 0x14 and 0x18 bytes before each
 * field respectively), the same idiom D_426F78 uses below for
 * dataExpTblGet; each points to a table of fixed-size records that
 * dataUnitInitGet, dataNormInitGet, dataSpecInitGet and dataEtherInitGet
 * index with a one-based id, as dataUnitOrgGet does orgData.
 */
extern int D_426F48;
extern int D_426F50;
extern int D_426F54;
extern int D_426F58;

void *dataUnitInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F48 - 8;
    value = D_426F48;
    return value + base + id * 0x34 - 0x34;
}

void *dataNormInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F50 - 0x10;
    value = D_426F50;
    return value + base + id * 0xC - 0xC;
}

void *dataSpecInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F54 - 0x14;
    value = D_426F54;
    return value + base + id * 0xC - 0xC;
}

void *dataEtherInitGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F58 - 0x18;
    value = D_426F58;
    return value + base + id * 0x18 - 0x18;
}

/*
 * D_426F78 is a selector field 0x38 bytes into a loaded battle-data table;
 * dataExpTblGet returns the table entry that selector picks out. Splat has
 * not named the table's own base, only this field.
 */
extern int D_426F78;

unsigned char *dataExpTblGet(void)
{
    return (unsigned char *)&D_426F78 - 0x38 + D_426F78;
}

/*
 * D_426F7C, D_426F80 and D_426F84 are the same kind of loaded-table byte
 * offset as D_426F48 above (0x3C, 0x40 and 0x44 bytes before their own table
 * start); dataParaTblGet, dataSpecTblGet and dataDefEquipGet each index a
 * table of 0x20-byte records with a one-based id.
 */
extern int D_426F7C;
extern int D_426F80;
extern int D_426F84;

void *dataParaTblGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F7C - 0x3C;
    value = D_426F7C;
    return value + base + id * 0x20 - 0x20;
}

void *dataDefEquipGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F84 - 0x44;
    value = D_426F84;
    return value + base + id * 0x20 - 0x20;
}

void *dataSpecTblGet(int id)
{
    unsigned char *base;
    int value;

    base = (unsigned char *)&D_426F80 - 0x40;
    value = D_426F80;
    return value + base + id * 0x20 - 0x20;
}

extern void dataEtherLearnSet(int etherType, int techniqueId, int selector);
const char D_00A456E8[] = "dataEtherTecSet: err %d\n";
extern CalcUnitParam *calcUPGet(ObjectTask *unit);
extern void *dataUnitFileGet(ObjectTask *unit, int charaId);
extern int dataUnitFileLoadMot(ObjectTask *unit, int motionId, int slot, void *fileInfo);

extern void dataSpecLearnSet(int cid, int specialId);

void dataEtherTecSet(int selector)
{
    int etherType = 0;
    int techniqueId = 0;
    int specialId = 0;

    switch (selector) {
    case 1:
        etherType = 3;
        techniqueId = 13;
        break;
    case 2:
        etherType = 3;
        techniqueId = 14;
        break;
    case 3:
        etherType = 3;
        techniqueId = 15;
        break;
    case 4:
        etherType = 3;
        techniqueId = 16;
        break;
    case 5:
        etherType = 6;
        techniqueId = 43;
        specialId = 90;
        break;
    case 6:
        etherType = 6;
        techniqueId = 46;
        specialId = 89;
        break;
    case 7:
        etherType = 7;
        techniqueId = 57;
        specialId = 97;
        break;
    case 8:
        etherType = 6;
        specialId = 88;
        break;
    default:
        printf(D_00A456E8, selector);
        return;
    }

    if (techniqueId != 0) {
        dataEtherLearnSet(etherType, techniqueId, selector);
    }
    if (specialId != 0) {
        dataSpecLearnSet(etherType, specialId);
    }
}

const char D_00A45730[] = "** dataEtherLearnSet: err %d\n";

void dataEtherLearnSet(int cid, int id, int selector)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45730, cid);
        return;
    }
    if (id != 0) {
        pl = dataPlChaGet(cid);
        b = id - 1;
        *(unsigned char *)((b / 8) + (int)pl + 0x28) |= (1 << (b % 8));
    }
}

const char D_00A45750[] = "** dataEtherLearnGet: err %d\n";

int dataEtherLearnGet(int cid, int id)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45750, cid);
        return 0;
    }
    pl = dataPlChaGet(cid);
    b = id - 1;
    return *(signed char *)((b / 8) + (int)pl + 0x28) & (1 << (b % 8));
}

const char D_00A45770[] = "** dataSkillLearnSet: err %d\n";

void dataSkillLearnSet(int cid, int id)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45770, cid);
        return;
    }
    if (id != 0) {
        pl = dataPlChaGet(cid);
        b = id - 1;
        *(unsigned char *)((b / 8) + (int)pl + 0x38) |= (1 << (b % 8));
    }
}

const char D_00A45790[] = "** dataSkillLearnGet: err %d\n";
const char D_00A457B0[] = "** dataSpecLearnSet: err %d\n";
extern PlCharacter *dataPlChaGet(int cid);

int dataSkillLearnGet(int cid, int id)
{
    PlCharacter *pl;
    int b;

    if (cid > 16) {
        printf(D_00A45790, cid);
        return 0;
    }
    pl = dataPlChaGet(cid);
    b = id - 1;
    return *(signed char *)((b / 8) + (int)pl + 0x38) & (1 << (b % 8));
}

typedef struct ThinkMapTable {
    unsigned char unmodeled_000[0x18C];
    int specBase[0x11]; /* +0x18C */
} ThinkMapTable;

extern ThinkMapTable thinkMapView __asm__("thinkMapTbl"); /* +0x18C runs on into specTbl */

int dataSpecBaseGet(int cid)
{
    return thinkMapView.specBase[cid];
}

/*
 * specTbl's tail is the character-id-indexed backup special-technique base
 * table dataBakpBaseGet reads.
 */


extern SpecTable specTbl;

int dataBakpBaseGet(int cid)
{
    return specTbl.bakpBase[cid];
}

void dataSpecLearnSet(int cid, int specialId)
{
    PlCharacter *pl;

    if (cid >= 0x11) {
        printf(D_00A457B0, cid);
        return;
    }
    if (specialId != 0) {
        pl = dataPlChaGet(cid);
        pl->special[specialId - dataSpecBaseGet(cid)].id = specialId;
    }
}

/*
 * thinkMapTbl's tail is a per-character-id table of special-technique base
 * ids; dataSpecLearnGet subtracts the caller's entry from a learned special
 * id before indexing PlCharacter.special, the same subtraction
 * dataSpecLearnSet performs through dataSpecBaseGet (still asm).
 */
const char D_00A457D0[] = "** dataSpecLearnGet: err %d\n";

short dataSpecLearnGet(int cid, int specialId)
{
    short learned;

    if (cid >= 0x11) {
        printf(D_00A457D0, cid);
        return 0;
    }
    learned = 0;
    if (specialId != 0) {
        learned = dataPlChaGet(cid)->special[specialId - thinkMapView.specBase[cid]].id;
    }
    return learned;
}

void *dataSpecDataGet(int cid, int id)
{
    PlCharacter *pl;
    int i;

    if (cid >= 0x11) {
        return 0;
    }
    if (id == 0) {
        return 0;
    }
    pl = dataPlChaGet(cid);
    for (i = 0; i < 8; i++) {
        if (pl->special[i].id == id) {
            return &pl->special[i];
        }
    }
    return 0;
}

extern const char D_00A456C8[];
extern unsigned char plChaData[];
extern const char D_00A45790[];
extern PlCharacter *dataPlChaGet(int cid);

int dataNormIdxGet(ObjectTask *unit, int id)
{
    int i;
    int off = 0x70;
    int ret = -1;
    for (i = 0; i < 6; i++) {
        if (*(short *)((char *)calcUPGet(unit) + off + 6) == id) {
            ret = i;
            break;
        }
        off += 2;
    }
    return ret;
}

typedef struct PlInitCopy12 { unsigned char b[0xC]; } PlInitCopy12;
typedef struct PlInitCopy24 { unsigned char b[0x18]; } PlInitCopy24;
typedef struct PlInitCopy34 { unsigned char b[0x34]; } PlInitCopy34;
typedef struct PlPackedInt { int v; } __attribute__((packed)) PlPackedInt;
typedef struct PlUnitSub {
    unsigned char pad0[4];
    unsigned short f4;
    unsigned short f6;
    unsigned short f8;
    unsigned char b[3];
    unsigned char padD;
    unsigned short s[3][3];
} PlUnitSub;
typedef struct PlUnitInit {
    unsigned short w0;
    unsigned short w2;
    unsigned char pad4[0x10];
    signed char level;
    unsigned char pad15[0x34 - 0x15];
} PlUnitInit;
typedef struct PlUnitRec {
    unsigned short w0;
    unsigned short w2;
    unsigned char pad4[0x10];
    signed char level;
    unsigned char pad15[0x30 - 0x15];
    PlUnitSub a;
    PlUnitSub b;
    unsigned char pad70[0x180 - 0x70];
} PlUnitRec;
typedef struct PlEquipSlot {
    unsigned short s0;
    unsigned char b;
    unsigned char pad;
    unsigned short s1;
} PlEquipSlot;
typedef struct PlChaSpec {
    short id;
    short pad2[2];
    short lv;
    unsigned char pad8[4];
} PlChaSpec;
typedef struct PlChaRec {
    unsigned char pad0[4];
    int nextExp;
    unsigned char pad8[0x10];
    unsigned short growth[8];
    unsigned char pad28[0x20];
    PlChaSpec spec[8];
} PlChaRec;
typedef struct PlShortTbl {
    short v[12];
} PlShortTbl;
typedef struct PlNormSlot { PlInitCopy12 norm; unsigned char pad[0x180 - 0xC]; } PlNormSlot;
extern PlNormSlot D_4229B6[];
extern int D_4229C2;
extern int D_4229CE;

void dataPlUnitInit(void)
{
  PlInitCopy12 *nb;
  unsigned char *cha;
  int i;
  int id;
  int k;
  int ofs;
  int ofs2;
  int j;
  PlUnitRec *rec;
  PlUnitSub *p;
  PlUnitSub *p2;
  unsigned char *q4;
  PlEquipSlot *eq;
  unsigned char *q;
  unsigned char *exp;
  unsigned short *para;
  PlInitCopy12 *sb;
  int *new_var;
  unsigned char *new_var2;
  unsigned short *d2;
  new_var = &D_4229CE;
  cha = plChaData;
  for (i = 0; i < 0x20; i++)
  {
    rec = &((PlUnitRec *) orgData)[i];
    nb = &D_4229B6[i].norm;
    memset(rec, 0, 0x180);
    id = i + 1;
    *((PlUnitInit *) rec) = *((PlUnitInit *) dataUnitInitGet(id));
    ofs = (i * 0x180) + 0x30;
    p = (PlUnitSub *) (((unsigned char *) orgData) + ofs);
    p->f4 = ((PlUnitInit *) rec)->w0;
    p->f6 = ((PlUnitInit *) rec)->w2;
    p->f8 = id;
    *nb = *((PlInitCopy12 *) dataNormInitGet(id));
    eq = dataDefEquipGet(id);
    d2 = rec->b.s[2];
    for (k = 0; k < 3; k++)
    {
      j = k * 6;
      q = ((unsigned char *) eq) + 2;
      q4 = ((unsigned char *) eq) + 4;
      rec->b.b[k] = q[j];
      d2[-6] = *((unsigned short *) (((unsigned char *) eq) + j));
      *d2 = *((unsigned short *) (q4 + j));
      d2++;
    }

    for (k = 0; k < 3; k++)
    {
      ((PlUnitRec *) (((unsigned char *) orgData) + (0x180 * i)))->b.s[1][k] = ((unsigned short *) (q + 0x10))[k];
    }

    new_var2 = (unsigned char *) new_var;
    ofs2 = (i * 0x180) + 0x50;
    p2 = ((unsigned char *) orgData) + ofs2;
    p2->f4 = *((unsigned short *) (((unsigned char *) eq) + 0x18));
    p2->f8 = *((unsigned short *) (((unsigned char *) eq) + 0x1A));
    p2->f6 = *((unsigned short *) (((unsigned char *) eq) + 0x1C));
    if (!eq)
    {
    }
    if (i < 0x10)
    {
      memset((void *) ((i * 0xA8) + ((int) cha)), 0, 0xA8);
      for (k = 7; k >= 0; k--)
      {
        ((short *) (cha + (i * 0xA8)))[(k * 6) + 0x27] = 2;
      }

      exp = ((unsigned char *(*)(int)) dataExpTblGet)(id);
      ((PlChaRec *) dataPlChaGet(id))->nextExp = ((PlPackedInt *) (exp + (((PlUnitRec *) (((unsigned char *) orgData) + (i * 0x180)))->level * 4)))->v;
      para = dataParaTblGet(id);
      for (k = 0; k < 8; k++)
      {
        ((PlChaRec *) dataPlChaGet(id))->growth[k] = ((PlShortTbl *) para)->v[k];
      }

      if (((!k) && (!k)) && (!k))
      {
      }
      if (!rec)
      {
      }
      for (k = 0; k < 6; k++)
      {
        dataSpecLearnSet(i + 1, ((PlShortTbl *) dataSpecInitGet(i + 1))->v[k]);
      }

      for (k = 0; ((k - 1) + 1) <= (12 - 1); k++)
      {
        ((void (*)(int, int)) dataEtherLearnSet)(i + 1, ((PlShortTbl *) dataEtherInitGet(i + 1))->v[k]);
      }

      sb = (PlInitCopy12 *) (((unsigned char *) (&D_4229C2)) + (i * 0x180));
      *sb = *((PlInitCopy12 *) dataSpecInitGet(id));
      *((PlInitCopy24 *) (new_var2 + (i * 0x180))) = *((PlInitCopy24 *) dataEtherInitGet(id));
      nb = &D_4229B6[i].norm;
    }
  }

}

void dataDummySet(void)
{
}

/*
 * playerData (7 entries) and enemyData (5 entries) are 0x180-byte battle
 * unit records; dataBattleInit is this TU's only writer and only clears the
 * calc-record index at +0x38.
 */
typedef struct BattleUnitInfo {
    unsigned char unmodeled_00[0x38];
    short calcIdx; /* +0x38 */
    unsigned char unmodeled_3A[0x146];
} BattleUnitInfo;

extern void dataFileInfoInit(void);
extern BattleUnitInfo playerData[7];
extern BattleUnitInfo enemyData[5];

void dataBattleInit(void)
{
    int i;

    dataFileInfoInit();
    for (i = 6; i >= 0; i--) {
        playerData[i].calcIdx = 0;
    }
    for (i = 4; i >= 0; i--) {
        enemyData[i].calcIdx = 0;
    }
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct BattleModelActor {
    BattleActor state;
    unsigned char unmodeled_88[0x8D0 - 0x88];
    void *modelAdr; /* +0x8D0 */
    void *animationAdr; /* +0x8D4 */
    void *textureAdr; /* +0x8D8 */
    /* dataUnitFileLoadMotSp2 indexes from +0x8DC; the extent is not yet
     * recovered. The overlapping motionAdr word is entry 1 at +0x8E0. */
    union {
        int slot[2];
        struct {
            int unmodeled_8dc;
            int motionAdr;
        } weapon;
    } motion;
} BattleModelActor;

typedef struct TecRecord {
    unsigned char unmodeled_00[0x11];
    signed char kind; /* +0x11 */
    unsigned char unmodeled_12[0x14 - 0x12];
    short motionId; /* +0x14 */
    unsigned char unmodeled_16[0x18 - 0x16];
} TecRecord;

typedef struct WpnRecord {
    unsigned char unmodeled_00[0x14];
    short tecId; /* +0x14 */
    unsigned char unmodeled_16[0x20 - 0x16];
} WpnRecord;

typedef struct AttRecord { unsigned char opaque[0x0E]; } AttRecord;

typedef struct EthRecord { unsigned char opaque[0x0C]; } EthRecord;

typedef struct ItmRecord { unsigned char opaque[0x14]; } ItmRecord;

typedef struct AccRecord { unsigned char opaque[0x10]; } AccRecord;

typedef struct FrmRecord { unsigned char opaque[0x10]; } FrmRecord;

typedef struct EngRecord { unsigned char opaque[0x0C]; } EngRecord;

typedef struct SklRecord { unsigned char opaque[0x08]; } SklRecord;

typedef struct GainRecord { unsigned char opaque[0x14]; } GainRecord;

typedef struct PartyLeaderView {
    unsigned char unmodeled_00[0x2C];
    unsigned short leader_battle_id; /* +0x2C */
} PartyLeaderView;

typedef struct PcPackEntry {
    int weaponId;
    char *name;
} PcPackEntry;

typedef struct CmdEntry {
    int selector; /* +0x00 */
    int argument; /* +0x04 */
    int extra;    /* +0x08: unwritten by this call */
    unsigned char unmodeled_0c[8]; /* +0x0C: unwritten by this call */
} CmdEntry;

extern BattleUnitInfo playerData2[7] __asm__("playerData");
const char D_00A457F0[] = "** dataUnitInitSet: err %d\n";
const char D_00A45810[] = "** dataUnitInitSet: err PL\n";
const char D_00A45830[] = "** dataUnitInitSet: err EN\n";
const char D_00A45850[] = "** dataUnitInitSet: err SKJ\n";
typedef struct UnitInitWork { unsigned char unmodeled_00[0x84]; BattleUnitInfo *info; } UnitInitWork;
typedef struct BattleUnitCopy { unsigned char b[0x180]; } BattleUnitCopy;
extern void calcCopyParaMake();
typedef struct BattleUnitSub { unsigned char pad0[4]; short f4; short f6; short id; } BattleUnitSub;
#define BSUB(p) ((BattleUnitSub *)((char *)(p) + 0x30))
extern void calcSkillInit();

void dataUnitInitSet(ObjectTask *unit, int cid)
{
    BattleUnitInfo *p = 0;
    UnitInitWork *w;
    int i;
    int src;
    BattleUnitInfo *pd;

    if (cid <= 0) {
        printf(D_00A457F0, cid);
        return;
    }
    w = *(UnitInitWork **)((char *)unit + 0x10);
    if (cid <= 0x20) {
        for (i = 0; i < 7; i++) {
            pd = playerData;
            if (BSUB(&playerData[i])->id == cid) {
                w->info = &pd[i];
                return;
            }
        }
        for (i = 0; i < 7; i++) {
            if (playerData[i].calcIdx == 0) {
                p = &playerData[i];
                break;
            }
        }
        if (p == 0) {
            printf(D_00A45810);
            return;
        }
        w->info = p;
        calcCopyParaMake(dataUnitOrgGet(cid), w->info);
        calcSkillInit(unit);
    } else if (cid < 0xBB) {
        for (i = 0; i < 5; i++) {
            if (enemyData[i].calcIdx == 0) {
                p = &enemyData[i];
                break;
            }
        }
        if (p == 0) {
            printf(D_00A45830);
            return;
        }
        w->info = p;
        memset(p, 0, 0x180);
        *(PlUnitInit *)p = *(PlUnitInit *)dataUnitInitGet(cid);
        BSUB(p)->f4 = ((PlUnitInit *)p)->w0;
        BSUB(p)->f6 = ((PlUnitInit *)p)->w2;
        *(PlInitCopy12 *)((char *)p + 0x76) = *(PlInitCopy12 *)dataNormInitGet(cid);
        *(PlInitCopy24 *)((char *)p + 0x8E) = *(PlInitCopy24 *)dataEtherInitGet(cid);
    } else {
        for (i = 0; i < 7; i++) {
            if (playerData[i].calcIdx == 0) {
                p = &playerData[i];
                break;
            }
        }
        if (p == 0) {
            printf(D_00A45850);
            return;
        }
        switch (cid - 0xBB) {
        case 0:
        case 1:
            src = 7;
            break;
        case 2:
            src = 6;
            break;
        case 3:
        case 4:
        case 5:
        case 6:
            src = 3;
            break;
        case 7:
            src = 2;
            break;
        default:
            printf(D_00A45850);
            return;
        }
        w->info = p;
        {
            BattleUnitInfo *q = playerData2;
            for (i = 0; i < 7; i++) {
                if (BSUB(&q[i])->id == src) {
                    *p = playerData2[i];
                }
            }
        }
        for (i = 2; i >= 0; i--) {
            ((short *)((char *)p + 0x5E))[i] = 0;
        }
    }
}

/*
 * posTbl is a table of 0x20-byte position records; dataPosTblGet and
 * dataPosLineGet both index it with the absolute value of the caller's
 * position id (negative ids alias the same record as their positive
 * counterpart). dataPosLineGet reads the halfword at +0x10 of the record.
 */


extern PosTblEntry posTbl[];

void *dataPosTblGet(int id)
{
    if (id < 0) {
        id = -id;
    }
    return &posTbl[id];
}

short dataPosLineGet(int id)
{
    if (id < 0) {
        id = -id;
    }
    return posTbl[id].line;
}

const char D_00A45890[] = "** dataCidGet: err %d\n";
extern short cidChgTbl[];

short dataCidGet(unsigned int cid)
{
    if (cid >= 0xC4) {
        printf(D_00A45890, cid);
        return 0;
    }
    return cidChgTbl[cid];
}

const char D_00A458A8[] = "** dataWidGet: err %d";
extern short widChgTbl[];

short dataWidGet(unsigned int wid)
{
    if (wid >= 0x75) {
        printf(D_00A458A8, wid);
        return 0;
    }
    return widChgTbl[wid];
}

extern int D_426F4C;
const char D_00A458C0[] = "** dataTecGet: err %d\n";

unsigned char *dataTecGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A458C0, index);
        return 0;
    }
    base = (unsigned char *)&D_426F4C - 0xC;
    return base + D_426F4C + ((index * 3) << 3) - 0x18;
}

const char D_00A458D8[] = "** dataWpnGet: err %d\n";
extern int D_426F5C;

unsigned char *dataWpnGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A458D8, index);
        return 0;
    }
    base = (unsigned char *)&D_426F5C - 0x1C;
    return base + D_426F5C + (index << 5) - 0x20;
}

const char D_00A458F0[] = "** dataAttGet: err %d\n";
extern int D_426F68;

unsigned char *dataAttGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A458F0, index);
        return 0;
    }
    base = (unsigned char *)&D_426F68 - 0x28;
    return base + D_426F68 + index * 14 - 0xE;
}

const char D_00A45908[] = "** dataEthGet: err %d\n";
extern int D_426F60;

unsigned char *dataEthGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45908, index);
        return 0;
    }
    base = (unsigned char *)&D_426F60 - 0x20;
    return base + D_426F60 + index * 12 - 0xC;
}

const char D_00A45920[] = "** dataItmGet: err %d\n";
extern int D_426F64;

unsigned char *dataItmGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45920, index);
        return 0;
    }
    base = (unsigned char *)&D_426F64 - 0x24;
    return base + D_426F64 + index * 20 - 0x14;
}

extern int D_426F6C;
const char D_00A45938[] = "** dataAccGet: err %d\n";

unsigned char *dataAccGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45938, index);
        return 0;
    }
    base = (unsigned char *)&D_426F6C - 0x2C;
    return base + D_426F6C + (index << 4) - 0x10;
}

const char D_00A45950[] = "** dataFrmGet: err %d\n";
extern int D_426F88;

unsigned char *dataFrmGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45950, index);
        return 0;
    }
    base = (unsigned char *)&D_426F88 - 0x48;
    return base + D_426F88 + (index << 4) - 0x10;
}

const char D_00A45968[] = "** dataEngGet: err %d\n";
extern int D_426F8C;

unsigned char *dataEngGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45968, index);
        return 0;
    }
    base = (unsigned char *)&D_426F8C - 0x4C;
    return base + D_426F8C + index * 12 - 0xC;
}

const char D_00A45980[] = "** dataSklGet: err %d\n";
extern int D_426F70;

unsigned char *dataSklGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45980, index);
        return 0;
    }
    base = (unsigned char *)&D_426F70 - 0x30;
    return base + D_426F70 + (index << 3) - 0x8;
}

const char D_00A45998[] = "** dataGainGet: err %d\n";
extern int D_426F74;

unsigned char *dataGainGet(int index)
{
    unsigned char *base;

    if (index <= 0) {
        printf(D_00A45998, index);
        return 0;
    }
    base = (unsigned char *)&D_426F74 - 0x34;
    return base + D_426F74 + ((((index << 2) + index)) << 2) - 0x294;
}

extern int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
const char D_00A459B0[] = "** read: err %s\n";
const char D_00A459C8[] = "** read: %s / %d\n";

int dataFileLoad(const char *name, void *dst)
{
    long size = xglCdReadFile(name, dst, 0, 0);
    if (size < 0) {
        printf(D_00A459B0, name);
        return 0;
    }
    printf(D_00A459C8, name, (int)size);
    return 1;
}

int dataCdSync(void)
{
    return cdReqNum;
}

void dataCdSyncClear(void)
{
    cdReqNum = 0;
}

extern int printf(const char *format, ...);
const char D_00A459E0[] = "** NBread: err %d\n";
const char D_00A459F8[] = "** NBread: end %d *\n";

void dataNBreadCB(int result) {
    if (result < 0) {
        printf(D_00A459E0, result);
        return;
    }
    if (result == 4) {
        if (--cdReqNum == 0) {
            printf(D_00A459F8, 0);
        }
    }
}

const char D_00A45A10[] = "** NBread: err %s\n";
const char D_00A45A28[] = "** NBread: (%d) %s = %X\n";

int dataFileLoadNB(const char *name, void *dst)
{
    long size = xglCdReadFile(name, dst, 1, (int)dataNBreadCB);
    if (size < 0) {
        printf(D_00A45A10, name);
        return 0;
    }
    cdReqNum++;
    printf(D_00A45A28, cdReqNum, name, dst);
    return 1;
}

typedef struct UfiEntry {
    int a;
    unsigned char *adr;
    int c;
} UfiEntry;
typedef struct UfiMot {
    int a;
    void *adr;
    int c;
    int unmodeled_c[3];
} UfiMot;
typedef struct UnitFileInfoView {
    int charaId;          /* +0x000 */
    UfiEntry mdl[3];      /* +0x004 */
    UfiMot mot[5];        /* +0x028 */
    UfiEntry tex[3][3];   /* +0x0A0 */
    UfiEntry face[3];     /* +0x10C */
} UnitFileInfoView;
extern UnitFileInfoView ufiView[6] __asm__("unitFileInfo");
#define U ufiView
extern void *motAdrTbl[];
extern unsigned char *mdlAdrTbl[];

void dataFileInfoInit(void)
{
    int i;
    int j;
    int k;

    for (i = 0; i < 6; i++) {
        U[i].charaId = 0;
        for (j = 0; j < 5; j++) {
            U[i].mot[j].a = 0;
            U[i].mot[j].adr = 0;
            U[i].mot[j].c = 0;
        }
        U[i].mot[0].adr = motAdrTbl[i];
        U[i].mot[4].adr = motAdrTbl[6];
        for (j = 0; j < 3; j++) {
            U[i].mdl[j].adr = mdlAdrTbl[i];
            U[i].mdl[j].a = 0;
            U[i].mdl[j].c = 0;
            U[i].face[j].a = 0;
            U[i].face[j].adr = 0;
            U[i].face[j].c = 0;
            for (k = 0; k < 3; k++) {
                U[i].tex[k][j].a = 0;
                U[i].tex[k][j].adr = 0;
                U[i].tex[k][j].c = 0;
            }
        }
    }
}

const char D_00A45A48[] = "** dataFileInfoInitChg: err\n";

void dataFileInfoInitChg(ObjectTask *unit, int charaId)
{
    char *file = dataUnitFileGet(unit, charaId);
    int i, j;
    char *p4, *p8, *pc, *p10;

    if (file == 0) {
        printf(D_00A45A48);
        return;
    }
    i = 0;
    p8 = file + 8;
    pc = file + 12;
    p10 = file + 16;
    *(int *)(file + 4) = 0;
    p4 = file + 4;
    for (; i < 3; i++) {
        int off = 0x38 + i * 24;
        *(int *)(p8 + off) = 0;
        *(float *)(pc + off) = 0.0f;
        *(int *)(p10 + off) = 0;
        for (j = 0; j < 3; j++) {
            int k = 0xa0 + i * 36 + j * 12;
            *(int *)(file + k) = 0;
            *(int *)(p4 + k) = 0;
            *(int *)(p8 + k) = 0;
        }
    }
}

typedef struct EnemyMdlHdr {
    unsigned int top;
    unsigned char *mid;
    unsigned char *end;
} EnemyMdlHdr;
typedef struct EnemyMdlRes {
    EnemyMdlHdr *address;
    int size;
    int pad[2];
} EnemyMdlRes;
extern EnemyMdlRes GameResource[];
const char D_00A45A68[] = "** src %X,%X,%X %d\n";
const char D_00A45A80[] = "** dst %X,%X,%X\n";
const char D_00A45A98[] = "** enemy Mdl found %d\n";
extern int GameResourceSearch(int type, int id);
extern void xglMemCopy16(unsigned int dst, unsigned int src, int n);
extern void xglMemCopy16b(unsigned int dst, unsigned int src, int n);
typedef struct EnemyMdlEnt {
    short id;
    short pad[3];
} EnemyMdlEnt;
typedef struct EnemyMdlFile {
    int charaId;
    int cid;
    unsigned char *base;
    int pad0c[2];
    unsigned char *mid;
    int pad18[2];
    unsigned char *end;
    unsigned char pad24[0x130 - 0x24];
} EnemyMdlFile;
extern EnemyMdlFile D_00A59420[];

void dataEnemyMdlChk(EnemyMdlEnt *tbl)
{
  EnemyMdlFile *dst;
  int i;
  int cid;
  int idx;
  EnemyMdlHdr *h;
  int sz;
  unsigned char *n;
  unsigned char **p;
  int id;
  for (i = 0; (tbl[i].id != 0) && (i < 3); i++)
  {
    id = dataCidGet(tbl[i].id);
    cid = id;
    idx = GameResourceSearch(0, cid);
    if (idx != (-1))
    {
      h = GameResource[idx].address;
      sz = GameResource[idx].size;
      printf(D_00A45A68, h->top, h->mid, h->end, sz);
      if (((((h->top != 0) && (h->mid != 0)) && (h->end != 0)) && (h->top < ((unsigned int) h->mid))) && (h->mid < h->end))
      {
        id = tbl[i].id;
        dst = &D_00A59420[i];
        do
        {
          n = dst->base;
        }
        while (0);
        dst->charaId = id;
        do
        {
          p = &h->mid;
          n += ((unsigned int) (*p)) - h->top;
          dst->cid = cid;
          dst->mid = n;
          p = &h->end;
          n += (*p) - h->mid;
          dst->end = n;
        }
        while (0);
        if (((unsigned int) dst->base) < h->top)
        {
          xglMemCopy16((unsigned int) dst->base, h->top, sz / 16);
        }
        else
        {
          xglMemCopy16b((((unsigned int) dst->base) + sz) - 0x10, (h->top + sz) - 0x10, sz / 16);
        }
        printf(D_00A45A80, dst->base, dst->mid, dst->end);
        printf(D_00A45A98, tbl[i].id);
        return;
      }
    }
  }

}

extern int leaderId;
extern int calcAccEquChk(ObjectTask *unit, int id);
typedef struct LeaderChkWork {
    unsigned char unmodeled_0[0x84];
    void *org; /* +0x84 */
    unsigned char unmodeled_88[0x88];
} LeaderChkWork;

void dataLeaderChk(short *ids)
{
  union 
  {
    ObjectTask task;
    unsigned char pad[0x80];
  } t;
  LeaderChkWork work;
  unsigned char *party;
  UnitFileInfo *base;
  int *p0;
  int i;
  party = PartyDataGet();
  dataLeaderCidReset();
  base = unitFileInfo;
  for (i = 0; i < 3; i++)
  {
    unsigned short cid;
    p0 = (int *) (base[0].unmodeled_4 + 4);
    p0 = (int *) ((*(unitFileInfo + 0)).unmodeled_4 + 4);
    t.task.work = &work;
    work.org = dataUnitOrgGet(ids[i * 4]);
    cid = *((unsigned short *) (party + 0x2C));
    if ((cid == dataCidGet(ids[i * 4])) && (calcAccEquChk(&t.task, 0x2D) == 0))
    {
      int tmp = *((int *) (base[i].unmodeled_4 + 4));
      int tmp0 = ((int *) base)[2];
      *((void **) p0) = (void *) tmp;
      *((unsigned int *) (&leaderCid)) = *((unsigned short *) (party + 0x2C));
      *((int *) (base[i].unmodeled_4 + 4)) = tmp0;
      leaderId = ids[(i * 2) * 2];
      *((int *) (base[i].unmodeled_4 + 4)) = tmp0;
      return;
    }
  }

}

int dataLeaderCidGet(void) {
    return leaderCid;
}

void dataLeaderCidReset(void) {
    leaderCid = 0;
}

extern unsigned char *PartyDataGet(void);
const char D_00A45AB0[] = "dataLeaderReload\n";

void dataLeaderReload(void)
{
    if (*(unsigned short *)(PartyDataGet() + 0x2C) != leaderCid) {
        printf(D_00A45AB0);
    }
}

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int dataMotNameGetSub(char *name, CalcUnitParam *up, int motionId, int weaponId);
extern char *motNameBase;
extern char *motNameExt;

int dataMotNameGetMenu(char *name, int unitOrgId, int weaponId)
{
    CalcUnitParam *up;
    int found;

    *name = 0;
    up = (CalcUnitParam *)dataUnitOrgGet(unitOrgId);
    if (up->sefSetupParams[weaponId] == 0) {
        return 0;
    }
    found = *(short *)(dataTecGet(*(short *)(dataWpnGet(up->sefSetupParams[weaponId]) + 0x14)) + 0x14);
    if (found == 0) {
        return 0;
    }
    strcpy(name, motNameBase);
    found = dataMotNameGetSub(name, up, found, weaponId);
    strcat(name, motNameExt);
    if (found == 0) {
        *name = 0;
    }
    return found;
}

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int dataMotNameGetSub(char *name, CalcUnitParam *up, int motionId, int weaponId);
extern char *motNameBase;
extern char *motNameExt;

int dataMotNameGet(char *name, ObjectTask *unit, int motionId, int weaponId)
{
    int found;

    strcpy(name, motNameBase);
    found = dataMotNameGetSub(name, calcUPGet(unit), motionId, weaponId);
    strcat(name, motNameExt);
    if (found == 0) {
        *name = 0;
    }
    return found;
}

extern char *motFile[];
extern char *agwsName[];
extern char *agwsLRName[];

int dataMotNameGetSub(char *name, CalcUnitParam *up, int motionId, int weaponId)
{
    char *mot;
    int lr;
    int i;
    int c;

    if (motionId < 0x157) {
        i = motionId;
        if (motionId >= 0x12E) {
            i = motionId - 0x2A;
        }
        mot = motFile[i - 1];
        if (mot != 0) {
            if (motionId >= 0x103 && (up->flags & 0x40)) {
                c = up->charaId - 0x11;
                if (motionId >= 0x12E) {
                    lr = 2;
                } else {
                    lr = (signed char)up->unmodeled_54[weaponId + 6] < 0x20;
                }
                strcat(name, agwsName[c]);
                strcat(name, mot);
                strcat(name, agwsLRName[lr]);
            } else {
                strcat(name, mot);
            }
            return 1;
        }
    }
    return 0;
}

extern int dataUnitFileLoadWep(ObjectTask *unit, int weaponId, int slot, void *fileInfo);
extern int dataUnitFileLoadMdl();
extern int leaderId;

int dataUnitFileLoad(ObjectTask *unit, int mode)
{
  void *file;
  int i;
  int off;
  unsigned char *base;
  short wpn;
  file = dataUnitFileGet(unit, mode);
  if (file == 0)
  {
    return 0;
  }
  if (leaderId == mode)
  {
    if (leaderId != calcUPGet(unit)->charaId)
    {
      dataLeaderCidReset();
    }
  }
  i = 0;
  mode = calcUPGet(unit)->charaId;
  dataUnitFileLoadMdl(unit, unitCidGet(unit), file);
  off = 0x50;
  do
  {
    base = ((unsigned char *) calcUPGet(unit)) + (off + (i * 2));
    wpn = *((short *) (base + 0xE));
    if ((wpn != 0) && ((*((signed char *) (dataTecGet(*((short *) (dataWpnGet(wpn) + 0x14))) + 0x11))) != 6))
    {
      dataUnitFileLoadWep(unit, wpn, i, file);
    }
    i += 1;
  }
  while (i < 3);
  dataUnitFileLoadMot(unit, mode, 0, file);
  i = 0;
  do
  {
    wpn = calcUPGet(unit)->sefSetupParams[i];
    if (((wpn != 0) && ((*((short *) (dataWpnGet(wpn) + 0xA))) == 1)) && ((*((signed char *) (dataTecGet(*((short *) (dataWpnGet(wpn) + 0x14))) + 0x11))) == 4))
    {
      dataUnitFileLoadMot(unit, *((short *) (dataTecGet(*((short *) (dataWpnGet(wpn) + 0x14))) + 0x14)), i + 1, file);
    }
    i += 1;
  }
  while (i < 3);
  return 1;
}

const char D_00A45AC8[] = "** dataUnitFileLoad: err\n";

void *dataUnitFileGet(ObjectTask *unit, int charaId)
{
    UnitFileInfo *slot;
    int lo;
    int hi;
    int i;

    slot = 0;
    for (i = 0; i < 6; i++) {
        if (unitFileInfo[i].charaId == charaId) {
            slot = &unitFileInfo[i];
            slot->charaId = calcUPGet(unit)->charaId;
            break;
        }
    }
    if (slot == 0) {
        if (charaId < 0x21 || charaId >= 0xBB) {
            lo = 0;
            hi = 3;
        } else {
            lo = 3;
            hi = 6;
        }
        for (i = lo; i < hi; i++) {
            if (unitFileInfo[i].charaId == 0) {
                slot = &unitFileInfo[i];
                slot->charaId = calcUPGet(unit)->charaId;
                break;
            }
        }
        if (slot == 0) {
            printf(D_00A45AC8);
        }
    }
    return slot;
}

typedef struct { int id; char *name; } PackMdlEnt;
const char D_00A45AE8[] = "data\\yamamoto\\pc\\";
const char D_00A45B00[] = "skj_jr";
const char D_00A45B08[] = "virgil";
const char D_00A45B10[] = "ziggy";
const char D_00A45B18[] = "shion";
const char D_00A45B20[] = "kosmos";
const char D_00A45B28[] = "chaos";
char *pcNameBase = D_00A45AE8;
PackMdlEnt pcPack_0[15] = { 1, D_00A45B28, 2, D_00A45B20, 3, D_00A45B18, 5, D_00A45B10, 6, D_00A455F8, 7, D_00A455F0, 0xA, D_00A45B08, 0xBB, D_00A45B00, 0xBC, D_00A45B00, 0xBD, D_00A44EA0, 0xBE, D_00A44E90, 0xBF, D_00A44E80, 0xC0, D_00A44E70, 0xC2, D_00A44E50, 0, 0 };
extern PackMdlEnt pcPack_0[];
extern int calcAccEquChk(ObjectTask *unit, int id);
extern int calcStatGet(ObjectTask *unit, int a, int b);
extern int MenuScenarioNoGet(void);

char *dataPackPcMdlNameGet(ObjectTask *unit)
{
    short temp_16;
    int temp_2;
    int var_5;
    char *var_18;

    if (*(unsigned int *)unit->work & 0x40) {
        goto block_8;
    }
    if (calcUPGet(unit)->flags & 0x40) {
        goto block_8;
    }
    if (calcUPGet(unit)->charaId < 0xBB) {
        goto block_16;
    }
    if (calcUPGet(unit)->charaId >= 0xC3) {
        goto block_16;
    }
block_8:
    if (calcUPGet(unit)->charaId >= 0xBB && calcUPGet(unit)->charaId < 0xC3) {
        goto block_16;
    }
    return 0;
block_16:
    var_18 = 0;
    temp_16 = calcUPGet(unit)->charaId;
    for (var_5 = 0; pcPack_0[var_5].id != 0; var_5++) {
        if (temp_16 == pcPack_0[var_5].id) {
            var_18 = pcPack_0[var_5].name;
            break;
        }
    }
    if (calcUPGet(unit)->charaId < 0xBB || calcUPGet(unit)->charaId >= 0xC3) {
        if (calcAccEquChk(unit, 0x2D) != 0) {
            if (temp_16 == 3) {
                var_18 = "shionm";
            } else if (temp_16 == 6) {
                var_18 = "momom";
            } else if (temp_16 == 1) {
                var_18 = "chaosm";
            } else if (temp_16 == 7) {
                var_18 = "jrm";
            }
        }
        if (temp_16 == 2 && MenuScenarioNoGet() < 0xA) {
            var_18 = "kosmos1";
        }
        if (calcStatGet(unit, 7, 2) != 0) {
            var_18 = "momoa";
        } else if (calcStatGet(unit, 7, 1) != 0) {
            var_18 = "momob";
        }
    }
    return var_18;
}

typedef struct { int id; char *name; } PackWpnEnt;
const char D_00A45B68[] = "data\\yamamoto\\wpn\\";
const char D_00A45B80[] = "son_mbox";
const char D_00A45B90[] = "jr_gun08";
const char D_00A45BA0[] = "jr_gun07";
const char D_00A45BB0[] = "jr_gun06";
const char D_00A45BC0[] = "jr_gun05";
const char D_00A45BD0[] = "jr_gun04";
const char D_00A45BE0[] = "jr_gun03";
const char D_00A45BF0[] = "jr_gun02";
const char D_00A45C00[] = "jr_gun01";
const char D_00A45C10[] = "zig_bw02";
const char D_00A45C20[] = "zig_bw01";
const char D_00A45C30[] = "zig_sw01";
const char D_00A45C40[] = "mmo_rd17";
const char D_00A45C50[] = "mmo_rd16";
const char D_00A45C60[] = "mmo_rd15";
const char D_00A45C70[] = "mmo_rd14";
const char D_00A45C80[] = "mmo_rd13";
const char D_00A45C90[] = "mmo_rd09";
const char D_00A45CA0[] = "mmo_rd08";
const char D_00A45CB0[] = "mmo_rd07";
const char D_00A45CC0[] = "mmo_rd06";
const char D_00A45CD0[] = "mmo_rd05";
const char D_00A45CE0[] = "mmo_rd04";
const char D_00A45CF0[] = "mmo_rd03";
const char D_00A45D00[] = "mmo_rd02";
const char D_00A45D10[] = "mmo_rd01";
const char D_00A45D20[] = "kos_bw04";
const char D_00A45D30[] = "kos_bw03";
const char D_00A45D40[] = "kos_bw02";
const char D_00A45D50[] = "kos_bw01";
const char D_00A45D60[] = "kos_hw06";
const char D_00A45D70[] = "kos_hw05";
const char D_00A45D80[] = "kos_hw04";
const char D_00A45D90[] = "kos_hw03";
const char D_00A45DA0[] = "kos_hw02";
const char D_00A45DB0[] = "kos_hw01";
const char D_00A45DC0[] = "cho_wing";
char *wpnNameBase = D_00A45B68;
PackWpnEnt pcPack_1[38] = { 0xB, D_00A45DC0, 0xC, D_00A45DB0, 0xD, D_00A45DA0, 0xE, D_00A45D90, 0xF, D_00A45D80, 0x10, D_00A45D70, 0x11, D_00A45D60, 0x12, D_00A45D50, 0x13, D_00A45D40, 0x14, D_00A45D30, 0x15, D_00A45D20, 0x18, D_00A45D10, 0x19, D_00A45D00, 0x1A, D_00A45CF0, 0x1B, D_00A45CE0, 0x1C, D_00A45CD0, 0x1D, D_00A45CC0, 0x1E, D_00A45CB0, 0x1F, D_00A45CA0, 0x20, D_00A45C90, 0x24, D_00A45C80, 0x25, D_00A45C70, 0x26, D_00A45C60, 0x27, D_00A45C50, 0x28, D_00A45C40, 0x29, D_00A45C30, 0x2A, D_00A45C20, 0x2B, D_00A45C10, 0x2E, D_00A45C00, 0x2F, D_00A45BF0, 0x30, D_00A45BE0, 0x31, D_00A45BD0, 0x32, D_00A45BC0, 0x33, D_00A45BB0, 0x34, D_00A45BA0, 0x35, D_00A45B90, 0x45, D_00A45B80, 0, 0 };
extern PackWpnEnt pcPack_1[];

char *dataPackWpnMdlNameGet(int id)
{
    char *name = 0;
    int i;
    for (i = 0; pcPack_1[i].id != 0; i++) {
        if (id == pcPack_1[i].id) {
            name = pcPack_1[i].name;
            break;
        }
    }
    return name;
}

/* Ported from upstream; local views keep this TU's existing types intact. */
typedef struct MdlLoadFile {
    int charaId;
    int modelCharaId; /* +0x04 */
    void *modelAdr; /* +0x08 */
    int modelSize; /* +0x0C */
    unsigned char unmodeled_10[4];
    void *animationAdr; /* +0x14 */
    int animationSize; /* +0x18 */
    unsigned char unmodeled_1c[4];
    void *textureAdr; /* +0x20 */
    int textureSize; /* +0x24 */
    unsigned char unmodeled_28[0x10C - 0x28];
    int faceCharaId; /* +0x10C */
} MdlLoadFile;
typedef struct MdlLoadActor {
    unsigned char unmodeled_00[0x8D0];
    void *modelAdr; /* +0x8D0 */
    void *animationAdr; /* +0x8D4 */
    void *textureAdr; /* +0x8D8 */
} MdlLoadActor;
typedef struct MdlLoadUnit {
    unsigned char unmodeled_00[0x10];
    int *work; /* +0x10: flags word first */
    MdlLoadActor *motionActor; /* +0x14 */
} MdlLoadUnit;
#define MDL_UP(unit) calcUPGet((ObjectTask *)(unit))

extern char mdlFileName[];
extern char mdlFileName2[];
extern char *pcNameBase;
extern const char D_00A44898[];
const char D_00A45DD0[] = "** dataUnitFileLoadMdl: Leader %d\n";
const char D_00A45DF8[] = ".lex";
const char D_00A45E00[] = ".xtx";
const char D_00A45E08[] = ".jnt";
const char D_00A45E10[] = "** dataUnitFileLoadMdl: hit %d / %X\n";
const char D_00A45E38[] = "** MDL read Size = %d\n";
extern char *dataPackPcMdlNameGet(ObjectTask *unit);
extern void RES_GetMdlFileName(char *name, int charaId);
extern int xglCdGetFileSize(const char *name);
extern int dataUnitFileLoadFaceMdl();
extern void ACT_initMotion();

int dataUnitFileLoadMdl(MdlLoadUnit *unit, int charaId, MdlLoadFile *file)
{
    MdlLoadActor *actor;
    char *packedName;

    actor = unit->motionActor;
    if (charaId != file->modelCharaId) {
        file->modelCharaId = charaId;
        if (((*unit->work & 0x40) == 0
             && (MDL_UP(unit)->flags & 0x40) == 0
             && (MDL_UP(unit)->charaId < 0xBB
                 || MDL_UP(unit)->charaId >= 0xC3))
            || (MDL_UP(unit)->charaId >= 0xBB
                && MDL_UP(unit)->charaId < 0xC3)) {
            if (dataLeaderCidGet() == charaId) {
                file->animationAdr = (void *)0x01070800;
                file->textureAdr = (void *)0x010B1000;
                printf(D_00A45DD0, charaId);
            } else {
                packedName = dataPackPcMdlNameGet((ObjectTask *)unit);
                strcpy(mdlFileName2, pcNameBase);
                strcat(mdlFileName2, packedName);
                strcat(mdlFileName2, D_00A44898);
                dataFileLoadNB(mdlFileName2, file->modelAdr);
                file->modelSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
                file->animationAdr = 0;
                file->textureAdr = 0;
            }
        } else {
            RES_GetMdlFileName(mdlFileName, charaId);
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45DF8);
            dataFileLoadNB(mdlFileName2, file->modelAdr);
            file->modelSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
            file->animationAdr = (unsigned char *)file->modelAdr + file->modelSize;
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E00);
            dataFileLoadNB(mdlFileName2, file->animationAdr);
            file->animationSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
            file->textureAdr = (unsigned char *)file->animationAdr + file->animationSize;
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E08);
            dataFileLoadNB(mdlFileName2, file->textureAdr);
            file->textureSize = ((xglCdGetFileSize(mdlFileName2) + 0x7FF) / 0x800) * 0x800;
        }
    } else {
        printf(D_00A45E10, charaId, file->modelAdr);
    }
    actor->modelAdr = file->modelAdr;
    actor->animationAdr = file->animationAdr;
    actor->textureAdr = file->textureAdr;
    ACT_initMotion(actor);
    printf(D_00A45E38, file->modelSize + file->animationSize + file->textureSize);
    if ((*unit->work & 0x40) == 0
        && (MDL_UP(unit)->flags & 0x40) == 0
        && (MDL_UP(unit)->charaId < 0xBB
            || MDL_UP(unit)->charaId >= 0xC3)) {
        dataUnitFileLoadFaceMdl(unit, charaId, file);
    } else {
        file->faceCharaId = 0;
    }
    return 1;
}

const char D_00A45E50[] = "** dataUnitFileLoadFaceMdl: hit %d / %X\n";

int dataUnitFileLoadFaceMdl(void *task, int cid, void *info) {
    char *p = (char *)info;
    char *a;

    if (cid != *(int *)(p + 0x10C)) {
        *(int *)(p + 0x10C) = cid;
        if (dataLeaderCidGet() == cid) {
            *(int *)(p + 0x110) = 0x010B4000;
            *(int *)(p + 0x128) = 0x010E6000;
        }
    } else {
        printf(D_00A45E50, cid, *(int *)(p + 0x110));
    }
    a = dataChildActorCreate(cid);
    *(int *)(a + 0x8D0) = *(int *)(p + 0x110);
    *(void **)((char *)task + 0x2C) = a;
    *(int *)(a + 0x8D4) = *(int *)(p + 0x14);
    *(int *)(a + 0x8D8) = *(int *)(p + 0x128);
    return 1;
}

extern char motFileName[];
const char D_00A45E80[] = "** FPK data nothing!\n";
const char D_00A45E98[] = "** dataUnitFileLoadMot: hit %d / %X\n";
const char D_00A45EC0[] = "** MOT read Size = %d %d\n";
typedef struct ActorWordsC {
    unsigned char unmodeled_0[0x100];
    int packEntry[4];
    unsigned char unmodeled_110[0x8D0 - 0x110];
    int resource[11];
} ActorWordsC;
typedef struct MotSlotView {
    unsigned char unmodeled_00[0x28];
    UfiMot motion[5];
} MotSlotView;

int dataUnitFileLoadMot(ObjectTask *unit, int motionId, int slot, void *fileInfo)
{
    MotSlotView *file = (MotSlotView *)fileInfo;
    char *actor;
    char *pack;
    int index;
    int i;

    actor = *(char **)((char *)unit + 0x14);
    if (motionId != file->motion[slot].a) {
        if (dataMotNameGet(motFileName, unit, motionId, slot - 1) == 0) {
            printf(D_00A45E80);
            return 0;
        }
        file->motion[slot].c = ((xglCdGetFileSize(motFileName) + 0x7FF) / 0x800) * 0x800;
        if (slot == 0 || slot == 4) {
            pack = file->motion[slot].adr;
        } else {
            pack = file->motion[0].adr;
            pack += (**(int **)((char *)unit + 0x10) & 0x40) == 0 ? 0x40000 : 0x32000;
            for (i = 1; i < 4; i++) {
                if (file->motion[i].a != 0 && file->motion[i].adr != 0
                    && (unsigned)file->motion[i].adr < (unsigned)pack) {
                    pack = file->motion[i].adr;
                }
            }
            pack -= file->motion[slot].c;
        }
        file->motion[slot].adr = pack;
        dataFileLoadNB(motFileName, pack);
        if (slot != 4) {
            file->motion[slot].a = motionId;
        }
    } else {
        printf(D_00A45E98, motionId, file->motion[slot].adr);
    }
    pack = file->motion[slot].adr;
    index = 3 + slot;
    ((ActorWordsC *)actor)->resource[index] = (int)pack;
    ((ActorWordsC *)actor)->packEntry[slot] = (int)pack;
    printf(D_00A45EC0, file->motion[slot].c, slot);
    return 1;
}

extern void *dataChildActorCreate(int callerId);
extern char *wpnNameBase;
const char D_00A45EE0[] = "** dataUnitFileLoadWep: hit %d / %X\n";
const char D_00A45F08[] = "** WEP read Size = %d\n";
const char D_00A45F20[] = "** wpnActot=%X %d\n";
struct WpnFileInfo;
extern int dataWpnLRChk(ObjectTask *unit, int weaponId, int slot, struct WpnFileInfo *fileInfo);
typedef struct WepFileEnt {
    int id;
    void *adr;
    int size;
} WepFileEnt;
typedef struct WepFileSlot {
    WepFileEnt part[3];
} WepFileSlot;
typedef struct WepFileInfo {
    unsigned char unmodeled_00[8];
    void *modelAdr; /* +0x08 */
    unsigned char unmodeled_0c[0xA0 - 0x0C];
    WepFileSlot wpn[3]; /* +0xA0 */
} WepFileInfo;
typedef struct WepActor {
    unsigned char unmodeled_00[0x698];
    int weaponLoadInfo; /* +0x698 */
    unsigned char unmodeled_69c[0x8D0 - 0x69C];
    void *resource[3]; /* +0x8D0 */
    unsigned char unmodeled_8dc[0x9A0 - 0x8DC];
    unsigned int drawEnvFlags; /* +0x9A0 */
} WepActor;
typedef struct WepUnit {
    unsigned char unmodeled_00[0x10];
    unsigned int *work;
    int motionActor;
    unsigned char unmodeled_18[4];
    int equipActor[4];
} WepUnit;
#define WPN_FILE_SECTORS(name) \
    (((xglCdGetFileSize(name) + (0x800 - 1)) / 0x800) * 0x800)
#define WEPF ((WepFileInfo *)fileInfo)

int dataUnitFileLoadWep(ObjectTask *unit, int weaponId, int slot, void *fileInfo)
{
    WepUnit *equip = (WepUnit *)unit;
    WepActor *actor;
    const char *packedName;
    int packed;
    unsigned char *address;
    int i;

    if (dataWidGet(weaponId) == 0x3000) {
        return 0;
    }
    if (weaponId != WEPF->wpn[slot].part[0].id) {
        packedName = dataPackWpnMdlNameGet(weaponId);
        if (packedName != 0) {
            packed = 1;
            strcpy(mdlFileName2, wpnNameBase);
            strcat(mdlFileName2, packedName);
            strcat(mdlFileName2, D_00A44898);
            WEPF->wpn[slot].part[0].size = WPN_FILE_SECTORS(mdlFileName2);
            WEPF->wpn[slot].part[1].size = 0;
            WEPF->wpn[slot].part[2].size = 0;
        } else {
            packed = 0;
            RES_GetMdlFileName(mdlFileName, dataWidGet(weaponId));
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45DF8);
            WEPF->wpn[slot].part[0].size = WPN_FILE_SECTORS(mdlFileName2);
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E00);
            WEPF->wpn[slot].part[1].size = WPN_FILE_SECTORS(mdlFileName2);
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E08);
            WEPF->wpn[slot].part[2].size = WPN_FILE_SECTORS(mdlFileName2);
        }
        if (!(calcUPGet(unit)->flags & 0x40) && slot == 2) {
            address = (unsigned char *)((int *)mdlAdrTbl)[6];
            WEPF->wpn[2].part[0].id = 0x3000;
        } else {
            address = WEPF->modelAdr;
            if (!(*equip->work & 0x40)) {
                address += 0x180000;
            } else {
                address += 0x100000;
            }
            for (i = 0; i < 3; i++) {
                if (WEPF->wpn[i].part[0].id != 0 && WEPF->wpn[i].part[0].adr != 0
                    && (unsigned char *)WEPF->wpn[i].part[0].adr < address) {
                    address = WEPF->wpn[i].part[0].adr;
                }
            }
            address -= WEPF->wpn[slot].part[0].size + WEPF->wpn[slot].part[1].size
                       + WEPF->wpn[slot].part[2].size;
            WEPF->wpn[slot].part[0].id = weaponId;
        }
        if (packed == 1) {
            if (dataWpnLRChk(unit, weaponId, slot, (struct WpnFileInfo *)fileInfo)) {
                WEPF->wpn[slot].part[0].adr = WEPF->wpn[0].part[0].adr;
            } else {
                WEPF->wpn[slot].part[0].adr = address;
                dataFileLoadNB(mdlFileName2, address);
            }
            WEPF->wpn[slot].part[1].adr = 0;
            WEPF->wpn[slot].part[2].adr = 0;
        } else if (dataWpnLRChk(unit, weaponId, slot, (struct WpnFileInfo *)fileInfo)) {
            WEPF->wpn[slot].part[0].adr = WEPF->wpn[0].part[0].adr;
            WEPF->wpn[slot].part[1].adr = WEPF->wpn[0].part[1].adr;
            WEPF->wpn[slot].part[2].adr = WEPF->wpn[0].part[2].adr;
        } else {
            WEPF->wpn[slot].part[0].adr = address;
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45DF8);
            dataFileLoadNB(mdlFileName2, WEPF->wpn[slot].part[0].adr);
            WEPF->wpn[slot].part[1].adr =
                (unsigned char *)WEPF->wpn[slot].part[0].adr + WEPF->wpn[slot].part[0].size;
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E00);
            dataFileLoadNB(mdlFileName2, WEPF->wpn[slot].part[1].adr);
            WEPF->wpn[slot].part[2].adr =
                (unsigned char *)WEPF->wpn[slot].part[1].adr + WEPF->wpn[slot].part[1].size;
            strcpy(mdlFileName2, mdlFileName);
            strcat(mdlFileName2, D_00A45E08);
            dataFileLoadNB(mdlFileName2, WEPF->wpn[slot].part[2].adr);
        }
    } else {
        printf(D_00A45EE0, weaponId, WEPF->wpn[slot].part[0].adr);
    }
    printf(D_00A45F08, WEPF->wpn[slot].part[0].size + WEPF->wpn[slot].part[1].size
                           + WEPF->wpn[slot].part[2].size);
    actor = dataChildActorCreate(dataWidGet(weaponId));
    actor->resource[0] = WEPF->wpn[slot].part[0].adr;
    actor->drawEnvFlags |= 0x200;
    actor->resource[1] = WEPF->wpn[slot].part[1].adr;
    equip->equipActor[slot & 3] = (int) actor;
    actor->resource[2] = WEPF->wpn[slot].part[2].adr;
    printf(D_00A45F20, actor, ((WepActor *)equip->motionActor)->weaponLoadInfo);
    return 1;
}
#undef WEPF

/*
 * dataWpnLRChk shares dataUnitFileLoadWep's own (ObjectTask *, weaponId,
 * slot, fileInfo) argument list, passed through unchanged; it never reads
 * the unit argument and only reaches the loaded file's weapon-id field 0xA0
 * bytes into fileInfo.
 */
typedef struct WpnFileInfo {
    unsigned char unmodeled_00[0xA0];
    int weaponId; /* +0xA0 */
} WpnFileInfo;

int dataWpnLRChk(ObjectTask *unit, int weaponId, int slot, WpnFileInfo *fileInfo)
{
    /*
     * The original guards both checks with a bnel to a shared fail exit
     * (0x00a1c1b4, 0x00a1c1c0); a structured cascade of "if (...) return 0;"
     * statements folds the second guard into a branchless compare instead.
     */
    if (slot != 1) {
        goto fail;
    }
    if (weaponId != fileInfo->weaponId) {
        goto fail;
    }
    return 1;
fail:
    return 0;
}

/*
 * The engine's actor record ACT_create hands out (main/near_dir.h and
 * main/set_motion.h model the same 0xa70-byte record as their own TU-local
 * copy: +0x00 flags, +0x04 an update callback, +0x08 a draw callback).
 * dataChildActorCreate is this TU's only writer of that head.
 */
typedef struct ChildActor {
    int flags;                               /* +0x00 */
    void (*update)(struct ChildActor *self); /* +0x04 */
    void (*draw)(struct ChildActor *self);   /* +0x08 */
} ChildActor;

extern ChildActor *ACT_create(int parent, int callerId);
extern void ACT_initMotion(ChildActor *actor);
const char D_00A45F38[] = "** dataChildActorCreate: err\n";

void *dataChildActorCreate(int callerId)
{
    ChildActor *actor;

    actor = ACT_create(-1, callerId);
    if (actor == 0) {
        printf(D_00A45F38);
        return 0;
    }
    ACT_initMotion(actor);
    actor->update = 0;
    actor->draw = 0;
    actor->flags = 0x208;
    return actor;
}

int dataUnitFileLoadMotSp(ObjectTask *unit, int motionId, int slot) {
    dataUnitFileLoadMot(unit, motionId, slot, dataUnitFileGet(unit, calcUPGet(unit)->charaId));
    return 1;
}

int dataUnitFileLoadMotSp2(UnitEquipInfo *unit, int motionId, int slot, int p3, int p4)
{
    EquipActor *actor;

    dataMotAdrSet(unit->motionTable, slot);
    if (p4 == 5) {
        actor = unit->equipActor[0];
        if (actor != 0) {
            actor->motionAdr = unit->motionTable->slot[slot];
        }
        actor = unit->equipActor[1];
        if (actor != 0) {
            actor->motionAdr = unit->motionTable->slot[slot];
        }
    } else if (p3 >= 0) {
        actor = unit->equipActor[p3 & 3];
        if (actor != 0) {
            actor->motionAdr = unit->motionTable->slot[slot];
        }
    }
    return 1;
}

const char D_00A45F58[] = "** dataUnitFileLoad2 %X\n";
extern void dataUnitFileLoad2Sub(void *unit);
extern int rnd(int max);
extern void unitCmdSet(void *unit, int *cmd);

void dataUnitFileLoad2(void *unit)
{
    int cmd[8];

    printf(D_00A45F58, unit);
    dataUnitFileLoad2Sub(unit);
    cmd[0] = 0x25;
    if (!(**(int **)((char *)unit + 0x10) & 0x40)) {
        cmd[1] = 0;
    } else {
        cmd[1] = rnd(10);
    }
    unitCmdSet(unit, cmd);
}

extern void ACT_setHand(void *actor, int hand);
extern void ACT_setFace(void *actor, void *other, int flags);
extern void unitFaceMotSet(void *unit, int motionId);
extern void unitWpnMotSet(void *unit, int slot, int a, int b);
extern void *sefCreateEffect2(void *param);
extern void dataSndSeRegLoad2(void *unit);
const char D_00A45F78[] = "** dataUnitFileLoad2Sub: Mdl Pack\n";
const char D_00A45FA0[] = "** dataUnitFileLoad2Sub: Wpn Pack\n";
void dataPackWpnMdl2(EquipActor *actor);
void ACT_setArms(void *actor, void *other, int acc_id, int flags);
void ACT_allocMatrix(void *actor, int count);
void ACT_setModelWrapper(void *actor, int flags);
void ACT_updateDefault(void *actor);

void dataUnitFileLoad2Sub(void *unit)
{
  unsigned char sp[0x24];
  EquipActor *mot;
  EquipActor *face;
  EquipActor *actor;
  unsigned char *pack;
  void *fx;
  int i;
  int off;
  int tec;
  short wid;
  mot = *((EquipActor **) (((char *) unit) + 0x14));
  face = *((EquipActor **) (((char *) unit) + 0x2C));
  if ((*((EquipActor **) (((char *) unit) + 0x14)))->wpnMdl[1] == 0)
  {
    int *hdr;
    int t;
    pack = mot->wpnMdl[0];
    hdr = (int *) pack;
    t = hdr[2];
    mot->wpnMdl[0] = pack + t;
    t = hdr[3];
    mot->wpnMdl[1] = pack + t;
    t = hdr[4];
    mot->wpnMdl[2] = pack + t;
    if (face != 0)
    {
      face->wpnMdl[1] = pack + hdr[3];
      t = hdr[5];
      face->wpnMdl[0] = pack + t;
      t = hdr[6];
      do
      {
        face->wpnMdl[2] = pack + t;
      }
      while (0);
    }
    printf(D_00A45F78);
  }
  i = 0;
  off = 0x50;
  do
  {
    if (sp)
    {
    }
    wid = *((short *) ((((char *) calcUPGet(unit)) + (off + (i * 2))) + 0xE));
    actor = ((EquipActor **) (((char *) unit) + 0x1C))[i & 3];
    i += 1;
    if (wid != 0x3000)
    {
      if (actor != 0)
      {
        if (actor->wpnMdl[1] == 0)
        {
          dataPackWpnMdl2(actor);
          printf(D_00A45FA0);
        }
      }
    }
  }
  while (i < 3);
  ACT_allocMatrix(mot, -1);
  ACT_setModelWrapper(mot, 0);
  if (((!((*(*((int **) (((char *) unit) + 0x10)))) & 0x40)) && (!(calcUPGet(unit)->flags & 0x40))) && ((calcUPGet(unit)->charaId < 0xBB) || (calcUPGet(unit)->charaId >= 0xC3)))
  {
    short hand;
    int sel;
    hand = *((short *) (((char *) mot) + 0x86));
    if (((hand == 4) || (hand == 0x14)) || ((sel = 0, hand == 0x15)))
    {
      sel = 1;
    }
    ACT_setHand(mot, sel);
  }
  dataMotAdrSet((MotionAdrTable *) mot, 0);
  if (face != 0)
  {
    ACT_setFace(face, mot, 0);
    ACT_allocMatrix(face, -1);
    ACT_setModelWrapper(face, 0);
    *((void **) (((char *) face) + 0x4)) = (void *) ACT_updateDefault;
    *((int *) (((char *) face) + 0x8DC)) = ((MotionAdrTable *) mot)->slot[0];
    *((int *) face) = (*((int *) face)) & (~8);
    unitFaceMotSet(unit, 0x35);
  }
  for (i = 0; i < 3; i++)
  {
    wid = calcUPGet(unit)->sefSetupParams[i];
    actor = ((EquipActor **) (((char *) unit) + 0x1C))[i & 3];
    if (wid != 0)
    {
      tec = ((signed char *) dataTecGet(*((short *) (dataWpnGet(wid) + 0x14))))[0x11];
      if ((*((short *) (dataWpnGet(wid) + 0xA))) == 1)
      {
        if (tec == 4)
        {
          dataMotAdrSet((MotionAdrTable *) mot, i + 1);
        }
      }
      off = 0x5A;
      if ((dataWidGet(wid) != 0x3000) && (0 != actor))
      {
        ACT_setArms(actor, mot, ((signed char *) calcUPGet(unit))[off + i], 0);
        ACT_allocMatrix(actor, -1);
        ACT_setModelWrapper(actor, 0);
        *((void **) (((char *) actor) + 0x4)) = (void *) ACT_updateDefault;
        if ((tec != 6) && (wid != 0xC))
        {
          *((int *) actor) = (*((int *) actor)) & (~8);
        }
        if (!((*(*((int **) (((char *) unit) + 0x10)))) & 0x40))
        {
          if (calcUPGet(unit)->flags & 0x40)
          {
            *((int *) (((char *) actor) + 0x8DC)) = ((int *) (((char *) mot) + 0x8E0))[i];
            unitWpnMotSet(unit, i, 0, 5);
          }
          else
          {
            *((int *) (((char *) actor) + 0x8DC)) = ((MotionAdrTable *) mot)->slot[0];
            unitWpnMotSet(unit, i, 0, 0xFF);
          }
        }
      }
    }
  }

  if ((((!((*(*((int **) (((char *) unit) + 0x10)))) & 0x40)) && (!(calcUPGet(unit)->flags & 0x40))) && ((calcUPGet(unit)->charaId < 0xBB) || (calcUPGet(unit)->charaId >= 0xC3))) || ((calcUPGet(unit)->charaId >= 0x97) && (calcUPGet(unit)->charaId < 0xBB)))
  {
    if ((calcStatGet(unit, 7, 2) == 0) && (calcStatGet(unit, 7, 1) == 0))
    {
      memset(sp, 0, 0x24);
      *((short *) (sp + 0x14)) = 0x4001;
      *((void **) sp) = *((void **) (((char *) unit) + 0x14));
      *((short *) (sp + 0x10)) = calcUPGet(unit)->charaId;
      fx = sefCreateEffect2(sp);
      *((void **) (((char *) calcUPGet(unit)) + 0x158)) = fx;
      if (fx != 0)
      {
        *((int *) (((char *) fx) + 0xA8C)) = (*((int *) (((char *) fx) + 0xA8C))) | 0x100;
      }
    }
  }
  else
  {
    *((void **) (((char *) calcUPGet(unit)) + 0x158)) = 0;
  }
  dataSndSeRegLoad2(unit);
}

/*
 * A packed model or animation file: 8 bytes not recovered, then one byte
 * offset per packed entry, counted from the start of the header, the same
 * idiom dataFpkAdrGet below resolves for a single entry.
 */
typedef struct PackedFile {
    unsigned char unmodeled_00[8];
    int entryOffset[3]; /* +0x08 */
} PackedFile;

void dataPackWpnMdl2(EquipActor *actor)
{
    PackedFile *pack;
    int offset;

    pack = (PackedFile *)actor->wpnMdl[0];
    offset = pack->entryOffset[0];
    actor->wpnMdl[0] = (unsigned char *)pack + offset;
    offset = pack->entryOffset[1];
    actor->wpnMdl[1] = (unsigned char *)pack + offset;
    offset = pack->entryOffset[2];
    actor->wpnMdl[2] = (unsigned char *)pack + offset;
}

typedef struct MotionAdrEntry {
    unsigned char unmodeled_00[8];
    int data_offset;
    int extra_offset;
} MotionAdrEntry;

void dataMotAdrSet(MotionAdrTable *table, int motionId)
{
    MotionAdrEntry *e;
    int d;
    int x;
    int *slot;
    int off = motionId << 2;

    off += 0x8dc;
    slot = (int *)((int)table + off);
    e = (MotionAdrEntry *)*slot;
    d = e->data_offset;
    x = e->extra_offset;
    *slot = (int)e + d;
    ((int *)((motionId << 2) + (int)table))[0x40] = (int)e + x;
}

/*
 * dataFpkAdrGet resolves a packed file's data pointer: 8 bytes not
 * recovered, then the offset of the packed data from the start of the
 * header.
 */
typedef struct FpkHeader {
    unsigned char unmodeled_00[8];
    int data_offset;
} FpkHeader;

void *dataFpkAdrGet(FpkHeader *fpk)
{
    return (unsigned char *)fpk + fpk->data_offset;
}

typedef struct WepSpEntry {
    int tid;
    int unk4;
    int unk8;
} WepSpEntry;

extern WepSpEntry wepSpTbl[];

WepSpEntry *dataTid2WepSp(int tid)
{
    int i;

    for (i = 0; wepSpTbl[i].tid != 0; i++) {
        if (wepSpTbl[i].tid == tid) {
            return &wepSpTbl[i];
        }
    }
    return 0;
}

int dataUnitFileLoadWep(ObjectTask *, int, int, void *);

int dataFileLoadWepSp(ObjectTask *unit, int tid)
{
    WepSpEntry *ent;
    void *fileInfo;
    int wid;

    fileInfo = dataUnitFileGet(unit, calcUPGet(unit)->charaId);
    ent = dataTid2WepSp(tid);
    wid = 2;
    if (ent == 0) {
        if (((signed char *)dataTecGet(tid))[0x11] == 6) {
            wid = 1;
        } else {
            return 0;
        }
    }
    if (wid == 2) {
        calcUPGet(unit)->sefSetupParams[2] = ent->unk4;
        calcUPGet(unit)->unmodeled_54[8] = ent->unk8;
        dataUnitFileLoadWep(unit, ent->unk4, 2, fileInfo);
        return 1;
    } else if (wid == 1) {
        dataUnitFileLoadWep(unit, calcUPGet(unit)->sefSetupParams[1], 1, fileInfo);
        return 1;
    }
    return 0;
}

extern void ACT_setArms(void *actor, void *other, int acc_id, int flags);
extern void ACT_allocMatrix(void *actor, int count);
extern void ACT_setModelWrapper(void *actor, int flags);
extern void ACT_updateDefault(void *actor);

int dataFileLoadWepSp2(ObjectTask *unit, int tid)
{
    int slot;
    void *parent;
    short wid;
    EquipActor *actor;

    slot = 2;
    if (dataTid2WepSp(tid) == 0) {
        slot = 1;
        if (((signed char *)dataTecGet(tid))[0x11] != 6) {
            return 0;
        }
    }
    parent = ((UnitEquipInfo *)unit)->motionTable;
    wid = calcUPGet(unit)->sefSetupParams[slot];
    actor = ((UnitEquipInfo *)unit)->equipActor[slot];
    if (wid != 0) {
        if (dataWidGet(wid) == 0x3000) {
            return 1;
        }
        if (actor == 0) {
            return 1;
        }
        if (actor->wpnMdl[1] == 0) {
            dataPackWpnMdl2(actor);
        }
        if ((unsigned)(wid - 0xD) >= 4) {
            ACT_setArms(actor, parent, *(signed char *)(slot + (int)calcUPGet(unit) + 0x5A), 0);
        }
        ACT_allocMatrix(actor, -1);
        ACT_setModelWrapper(actor, 0);
        *(void **)((char *)actor + 4) = (void *)ACT_updateDefault;
    }
    return 1;
}

extern void ACT_dispose(void *);

int dataFileLoadWepSpEnd(ObjectTask *unit, int tid) {
    int slot;
    void **pp;
    int off;

    slot = 2;
    if (dataTid2WepSp(tid) == 0) {
        slot = 1;
        if (((signed char *)dataTecGet(tid))[0x11] != 6) {
            return 0;
        }
    }
    off = slot * 4 + 0x1C;
    pp = (void **)((char *)unit + off);
    if (*pp != 0) {
        ACT_dispose(*pp);
        *pp = 0;
    }
    if (slot == 2) {
        calcUPGet(unit)->sefSetupParams[2] = 0;
        calcUPGet(unit)->unmodeled_54[8] = 0;
    }
    return 1;
}

/* Callers pass the unit whose default weapon they want; the function
 * always returns 0 and ignores it. */
int dataDefWpnGet(void *unit)
{
    return 0;
}

const char D_00A45FC8[] = "** MTD id=%d frame=%d **\n";

int dataMtdRead(void *unit, short trigger)
{
    unsigned char *act;
    unsigned char *base;
    unsigned char *rec;
    unsigned char *r;
    short *p;
    unsigned int off;
    int idx;
    int id;
    int i;
    unsigned int v;
    short cnt;
    int sk;
    short n;
    float ft;
    int frame;
    int key;
    int k;

    act = *(unsigned char **)((unsigned char *)unit + 0x14);
    idx = (*(unsigned short *)(act + 0x704) & 0x700) >> 8;
    id = (*(unsigned short *)(act + 0x704) & 0xFF) + 1;
    if (idx >= 5) {
        return 0;
    }
    base = *(unsigned char **)(act + 0x100 + idx * 4);
    if (base == 0) {
        return 0;
    }
    off = *(short *)base;
    p = (short *)base + (off >> 1);
    rec = 0;
    for (i = 0; (sk = p[i]) != 0; i += 2) {
        if (sk == id) {
            v = p[i + 1];
            rec = base + (((unsigned int)v >> 1) << 1);
            break;
        }
    }
    if (rec == 0) {
        return 0;
    }
    r = rec;
    key = *(unsigned short *)r;
    ft = *(float *)(act + 0x6F4) / 0.033333335f + 0.5f;
    frame = (int)ft;
    k = key;
    while (k != 0) {
        {
            if ((short)k == trigger) {

                for (i = 0; i < *(short *)(r + 2); i++) {
                    idx = *(short *)(r + 4 + (i << 1));
                    if (idx == frame) {
                        printf(D_00A45FC8, (short)key);
                        return 1;
                    }
                }
                return 2;
            }
            n = *(short *)(r + 2);
            rec = (unsigned char *)((short *)rec + n + 2);
            r = rec;
            key = k = *(unsigned short *)r;
        }
    }
    return 0;
}



extern XtxFileEntry xtxFile[];

void *dataXtxFileGet(int index)
{
    return &xtxFile[index];
}

const char D_00A45FE8[] = "** dataXtxAdrGet: err %d\n";
extern unsigned short xtxAdr[0x13];

unsigned short dataXtxAdrGet(int index)
{
    unsigned short value;

    value = 0;
    if (index < 0x13) {
        if (index >= 0) {
            value = xtxAdr[index];
        } else {
            goto fail;
        }
    } else {
    fail:
        printf(D_00A45FE8, index);
    }
    return value;
}

typedef struct { char *name; void *adr; } XtxLoadEnt;

extern int dataFileLoadNB(const char *name, void *dst);

void dataXtxLoad(void)
{
    int i = 0;

    while (((XtxLoadEnt *)xtxFile)[i].name != 0) {
        ((int (*)(void *, void *))dataFileLoadNB)(((XtxLoadEnt *)xtxFile)[i].name, ((XtxLoadEnt *)xtxFile)[i].adr);
        i++;
    }
}

extern int dataFileLoad(const char *name, void *dst);

const char D_00A46008[] = "data\\yamamoto\\batdat.bin";
extern unsigned char batDatBuf[0x10000];

void dataBatDatLoad(void)
{
    dataFileLoad(D_00A46008, batDatBuf);
}

extern unsigned char thinkBuf[0x4000];

void *dataThinkAdrGet(void)
{
    return thinkBuf;
}

int thinkMapGet(int id)
{
    int i;
    for (i = 0; i < 100; i++) {
        if (id == ((int *)&thinkMapTbl)[i]) {
            return i;
        }
    }
    return -1;
}

extern int thinkMapGet(int mapNo);
extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int sprintf(char *buffer, const char *format, ...);
extern char *thinkName;
extern char *thinkNameBase;
extern char *thinkNameExt;
const char D_00A46028[] = "%03d";

int dataThinkNameGet(char *name, int thinkNo)
{
    char num[16];
    int mappedNo;

    mappedNo = thinkMapGet(thinkNo);
    if (mappedNo != -1) {
        strcpy(name, thinkNameBase);
        strcat(name, thinkName);
        sprintf(num, D_00A46028, mappedNo);
        strcat(name, num);
        strcat(name, thinkNameExt);
        return 1;
    }
    name[0] = '\0';
    return 0;
}

extern void thinkNoSet(int);

int dataThinkDataSet(int thinkNo, const void *data)
{
    int mappedNo;

    mappedNo = thinkMapGet(thinkNo);
    if (mappedNo != -1 && data != 0) {
        __builtin_memcpy(thinkBuf, data, sizeof(thinkBuf));
        thinkNoSet(mappedNo);
        return 1;
    }
    return 0;
}

extern void thinkNoSet(int);

int dataThinkFileLoad(int thinkNo)
{
    int no = thinkMapGet(thinkNo);
    if (no == -1)
        return 0;
    dataThinkLoad(no);
    thinkNoSet(no);
    return 1;
}

extern char *thinkName;
extern char *thinkNameBase;
extern char *thinkNameExt;
extern const char D_00A46028[];

void dataThinkLoad(int thinkNo)
{
    char path[256];
    char num[16];

    strcpy(path, thinkNameBase);
    strcat(path, thinkName);
    sprintf(num, D_00A46028, thinkNo);
    strcat(path, num);
    strcat(path, thinkNameExt);
    dataFileLoad(path, thinkBuf);
}

/* dataMapLoad passes the map number it is loading; every map loads into
 * the same fixed EE RAM region, so the number is ignored. */
void *dataMapLoadAdrGet(int mapNo)
{
    return (void *)0x01b9e000;
}

typedef struct MapHeader {
    unsigned char unmodeled_00[0x10];
    int cameraOffset; /* +0x10 */
} MapHeader;

void *dataMapCameraAdrGet(int mapNo)
{
    MapHeader *map = dataMapLoadAdrGet(mapNo);
    return (unsigned char *)map + map->cameraOffset;
}

extern char *strcpy(char *destination, const char *source);
extern char *strcat(char *destination, const char *source);
extern int sprintf(char *buffer, const char *format, ...);
extern char *mapNameBase;
extern char *mapName;
extern char *mapNameExt;
extern const char D_00A46028[];
const char D_00A46030[] = "** dataMapLoad: err -> %d\n";
void dataMapLoad(int mapNo)
{
    char path[256];
    char num[16];

    if (mapNo < 100) {
        strcpy(path, mapNameBase);
        strcat(path, mapName);
        sprintf(num, D_00A46028, mapNo);
        strcat(path, num);
        strcat(path, mapNameExt);
        dataFileLoadNB(path, dataMapLoadAdrGet(mapNo));
        return;
    }
    printf(D_00A46030, mapNo);
}

void dataVPadModeSet(int mode)
{
    padMode = mode;
    padData = 0;
}

extern u16 padConvTbl[];

void dataVPadSet(u16 pad)
{
    int i;
    u16 r = 0;
    for (i = 0; i < 14; i++) {
        if ((pad >> i) & 1) {
            r |= padConvTbl[i];
        }
    }
    padData = r;
}

extern int D_426F4C;
extern const char D_00A458C0[];
extern const char D_00A458D8[];
extern int D_426F5C;
extern const char D_00A45980[];
extern int D_426F70;
typedef struct WpnLRFileInfo {
    unsigned char unmodeled_00[0xA0];
    int wpnId; /* +0xA0 */
} WpnLRFileInfo;
extern const char D_00A45F58[];
extern void dataUnitFileLoad2Sub(void *unit);
extern int rnd(int max);
extern void unitCmdSet(void *unit, int *cmd);
extern void thinkNoSet(int);
extern u16 padConvTbl[];
extern unsigned short PadData[];

unsigned short dataPadRead(int arg0) {
    unsigned short r = 0;
    if (padMode == 0) {
        switch (arg0) {
        case 0: r = PadData[0x18]; break;
        case 1: r = PadData[0x19]; break;
        case 2: r = PadData[0x1a]; break;
        }
    } else {
        r = padData;
    }
    return r;
}

WepSpEntry wepSpTbl[16] = { 0x30, 0xB, 0x1B, 0x35, 0xD, 0, 0x36, 0xE, 0, 0x37, 0xF, 0, 0x3A, 0x10, 0, 0x3B, 0x11, 0x20, 0x53, 0x24, 0x20, 0x54, 0x25, 0x20, 0x56, 0x26, 0x20, 0x57, 0x25, 0x20, 0x58, 0x27, 0x20, 0x5A, 0x28, 0x20, 0x5C, 0x33, 0x20, 0x5F, 0x33, 0x20, 0x61, 0x34, 0x20, 0, 0, 0 };
u16 padConvTbl[14] = { 0x1000, 0x4000, 0x8000, 0x2000, 0x10, 0x40, 0x80, 0x20, 4, 1, 8, 2, 0x800, 0x100 };

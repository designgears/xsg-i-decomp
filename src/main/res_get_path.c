#include "common.h"
#include "shared.h"

static char *command_enenpcse(int command, char *cursor, int mode);
extern void GameDiskChange(int disk_number);
extern void Intermission(int save_number);
extern int xglCdGetFileSize(const char *name);
/* .sbss (file-local, original order) */
static u32 *arcfileaddr;
static u8 arcno;
static int arcfilepreload;
static u8 GameResourceWorkReloadLeader;
extern void SCRIPT_fade(int time);
static char *command_mpeg2_core(int command, char *cursor, int mode);
static int next_arc_size(void *address);
extern void Enemy_LoadPreset(void *address, const char *name);
extern void *AdrsEnemyPreset;
#include "main/xgl_thread.h"

/* Types recovered upstream (origin/master); kept for reference. */
/* search_key walks this object in 12-byte records: it compares the first
 * string with the requested key and walks the string-pointer list at +4.
 * The third pointer is present in every record but is not read by the
 * recovered caller, so its role remains intentionally unnamed. */
typedef struct ResourceKeyEntry {
    const char *key;
    const void *aliases;
    const void *unmodeled_08;
} ResourceKeyEntry;


typedef struct {
    u16 id;
    u8 type;
    u8 foot;
    char name[16];
} EnemySeEntry;

/* .bss (file-local, original order) */
static EnemySeEntry EnemySeBank[8];
static char foot_name[0x10];
static char env_name[0x10];
static char scene_txt_buffer[0x800];
static u32 archeader[0x40];
static u8 GameResourceWorkReloadTable[0x80];
extern u8 ScenePath;
/* Resource-key tables: {key, table, extension}; `table` is a NULL-terminated
 * name list or (CID/RID/... in `model`) another key table. */
typedef struct ResKeyTable {
    const char *key;
    const void *table;
    const char *ext;
} ResKeyTable;
typedef struct {
    char *name;
    u16 ids[14];
} CfMtnEntry;
typedef struct { unsigned short *tbl; unsigned short en; unsigned short key; } FaceEnt;

static const char *CID0[54] = {
    "+shion,shion4", "kosmos", "chaos", "momo",
    "jr", "ziggy", "shitan", "shion_ch",
    "+shion1,shion5", "shion2", "shion3", "shion",
    "+shion5,shion1", "kosmos1", "kosmos2", "kosmos3",
    "kosmos5", "chaos1", "chaos2", "momo1",
    "momo2", "momo3", "momo4", "jr1",
    "jr2", "jr3", "ziggy1", "shitan1",
    "shitan2", "+shion_h,shion4_h", "kosmos_h", "chaos_h",
    "momo_h", "jr_h", "ziggy_h", "shitan_h",
    "kosmos_h1", "kosmos_h2", "kosmos_h3", "kosmos_h4",
    "kosmos_h5", "kosmos_h6", "+shion1_h,shion5_h", "jr1_h",
    "shion2_h", "shion3_h", "jr4", "+shion4_h,shion_h",
    "+shion5_h,shion1_h", "momo5", "momo5_h", "momo6",
    "shion6_h", 0
};
static const char *CID1[89] = {
    "marglis", "marglis1", "gaignun", "gaignun1",
    "albelt", "albelt1", "allen", "allen1",
    "kebin", "kebin1", "kebin2", "virgil",
    "virgil1", "virgil2", "virgil3", "virgil4",
    "fried", "elly", "matehws", "matehws1",
    "tonny", "hammer", "andrew", "andrew1",
    "andrew2", "andrew3", "andrew4", "feb",
    "yuri", "joachim", "sellers", "mary",
    "shelley", "abel", "helmer", "voyager",
    "miyuki", "lapis", "cecilia", "cath",
    "shi_dad", "shi_mam", "step_mam", "pelegri",
    "togashi", "moriyama", "hynlin", "shalron",
    "k_joach", "and_wife", "and_daug", "elly2",
    "van", "chief", "cecilia1", "cath1",
    "joachim1", "marglis_h", "albelt_h", "allen_h",
    "kebin_h", "virgil_h", "fried_h", "+matehws_h,matehws",
    "elly_h", "kebin1_h", "virgil2_h", "virgil4_h",
    "andrew5", "andrew6", "andrew7", "andrew8",
    "albelt2_h", "virgil1_h", "virgil3_h", "+andrew_h,andrew",
    "+andrew_h,andrew1", "+andrew_h,andrew2", "+andrew_h,andrew3", "+andrew_h,andrew4",
    "+andrew_h,andrew5", "+andrew_h,andrew6", "gaignun_h", "+tonny_h,tonny",
    "+hammer_h,hammer", "feb1", "albelt3_h", "andrew9",
    0
};
static const char *CID2[32] = {
    "space", "vec_m", "vec_m1", "vec_m2",
    "vec_w", "vec_w1", "off_m", "off_m1",
    "off_m2", "off_m3", "off_w", "off_w1",
    "pilot", "sold", "eng", "eng_off",
    "suuki", "rw_m", "rw_m1", "rw_m2",
    "rw_w", "rw_w1", "rw_w2", "esold",
    "breal_m", "breal_m1", "breal_m2", "breal_m3",
    "breal_m4", "breal_w", "breal_w1", 0
};
static const char *CID3[29] = {
    "commi", "commi1", "commi2", "commi3",
    "commi4", "commi5", "gove_w", "gove_w1",
    "gove_w2", "u_sold", "u_space", "real_m",
    "real_m1", "real_m2", "real_w", "real_w1",
    "real_w2", "real_w2", "real_w2", "real_w2",
    "real_w2", "real_w2", "real_w2", "real_w2",
    "real_w2", "real_w2", "real_w2", "u_solds",
    0
};
static const char *CID4[17] = {
    "du", "du1", "du2", "hyaku",
    "ass_m", "ass_m1", "ass_m2", "ass_w",
    "gove_m", "gove_m1", "gove_m2", "gove_m2",
    "gove_m2", "kancho", "ass_m3", "ass_m4",
    0
};
static const char *CID5[10] = {
    "cont", "utic_m", "utic_m1", "utic_m2",
    "utic_w", "utic_w1", "utic_w2", "pol_m",
    "pol_w", 0
};
static const char *CID6[81] = {
    "man_a", "man_a1", "man_a2", "man_b",
    "man_b1", "man_b2", "oji_a", "oji_a1",
    "oji_a2", "oji_b", "oji_b1", "oji_b2",
    "jiji_a", "jiji_a1", "jiji_a2", "jiji_b",
    "jiji_b1", "jiji_b2", "kidsm_a", "kidsm_a1",
    "kidsm_a2", "kidsm_b", "kidsm_b1", "kidsm_b2",
    "boy_a", "boy_a1", "boy_a2", "boy_b",
    "boy_b1", "boy_b2", "wman_a", "wman_a1",
    "wman_a2", "wman_b", "wman_b1", "wman_b2",
    "oba_a", "oba_a1", "oba_a2", "oba_b",
    "oba_b1", "oba_b2", "baba_a", "baba_a1",
    "baba_a2", "baba_b", "baba_b1", "baba_b2",
    "kidsw_a", "kidsw_a1", "kidsw_a2", "kidsw_b",
    "kidsw_b1", "kidsw_b2", "girl_a", "girl_a1",
    "girl_a2", "girl_b", "girl_b1", "girl_b2",
    "bread", "bar_oji", "bar_m", "doctor",
    "nurse", "cle_ch", "robot", "king",
    "cle_oba", "bread_ch", "inn_girl", "mouth",
    "goro", "pilot_no", "king_cat", "barten",
    "spy", "inn_oji", "scot", "eng_oji",
    0
};
static const char *RID0[7] = {
    "01", "02", "03", "04",
    "05", "06", 0
};
static const char *RID1[14] = {
    "_01", "_02", "_03", "_04",
    "_05", "_01_cv", "_01_ho", "_01_ho2",
    "_02_ho", "_06", "_07", "_08",
    "_09", 0
};
static const char *RID2[11] = {
    "ukn_g", "vx_1m", "ts_01", "ukunB",
    "ukunS", "gr_final", "gr_head", "gr_hand",
    "gr_body", "gr_leg", 0
};
static const char *WID0[11] = {
    "hw01", "hw02", "hw03", "hw04",
    "hw05", "hw06", "bw01", "bw02",
    "bw03", "bw04", 0
};
static const char *WID1[18] = {
    "rd01", "rd02", "rd03", "rd04",
    "rd05", "rd06", "rd07", "rd08",
    "rd09", "rd10", "rd11", "rd12",
    "rd13", "rd14", "rd15", "rd16",
    "rd17", 0
};
static const char *WID2[6] = {
    "sw01", "bw01", "bw02", "bw03",
    "bw04", 0
};
static const char *WID3[9] = {
    "gun01", "gun02", "gun03", "gun04",
    "gun05", "jr_hg06", "jr_hg07", "jr_hg08",
    0
};
static const char *WID4[6] = {
    "sw01", "sw02", "sw03", "sw04",
    "sw05", 0
};
static const char *WID5[2] __attribute__((section(".data"))) = {
    "mbox", 0
};
static const char *WID6[50] = {
    "ax00", "bd00", "bd01", "wct00",
    "hg00", "hg01", "hmr00", "dlc00",
    "hmr02", "mg00", "mg01", "rf00",
    "rf01", "rf02", "bam00", "bs00",
    "drl00", "drl01", "lsr00", "pbk00",
    "fl00", "gnd00", "glg00", "hb00",
    "msp00", "msp02", "bl00", "cb00",
    "eac00", "bp00", "lgc00", "amp02",
    "msp01", "msp03", "shb00", "fsd00",
    "vsd00", "bbc00", "bw00", "ead00",
    "ecm01", "ecm02", "smp01", "wire00",
    "sol00", "deff00", "glg00r", "hb00r",
    "msp02r", 0
};
static const char *WID7[12] = {
    "acc018", "acc028", "acc029", "acc030",
    "acc031", "acc032", "acc033", "acc034",
    "acc035", "acc036", "ewp_hg01", 0
};
static const char *WID8[2] __attribute__((section(".data"))) = {
    "wing", 0
};
static const char *EID0[56] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", "011", "012",
    "013", "014", "015", "016",
    "017", "018", "019", "020",
    "021", "022", "023", "024",
    "025", "301", "302", "303",
    "304", "305", "306", "307",
    "308", "309", "310", "311",
    "312", "313", "314", "315",
    "316", "317", "318", "319",
    "320", "321", "322", "323",
    "324", "325", "", "",
    "", "", "114", 0
};
static const char *EID1[15] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", "011", "012",
    "013", "014", 0
};
static const char *EID2[7] = {
    "001", "002", "003", "004",
    "005", "006", 0
};
static const char *EID3[8] = {
    "001", "002", "003", "004",
    "005", "006", "007", 0
};
static const char *EID4[8] = {
    "001", "002", "003", "004",
    "005", "006", "007", 0
};
static const char *EID5[7] = {
    "001", "002", "003", "004",
    "005", "006", 0
};
static const char *EID6[2] __attribute__((section(".data"))) = {
    "001", 0
};
static const char *EID7[5] = {
    "001", "002", "003", "004",
    0
};
static const char *EID8[4] = {
    "001", "002", "003", 0
};
static const char *EIDf[34] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", "011", "012",
    "013", "014", "015", "016",
    "017", "018", "019", "020",
    "021", "022", "023", "024",
    "025", "026", "027", "028",
    "029", "030", "031", "032",
    "033", 0
};
static const char *MID0[182] = {
    "000", "001", "002", "003",
    "004", "005", "006", "007",
    "008", "009", "010", "011",
    "012", "013", "014", "015",
    "016", "017", "018", "019",
    "020", "021", "022", "023",
    "024", "025", "026", "027",
    "028", "029", "030", "031",
    "032", "033", "034", "035",
    "036", "037", "038", "039",
    "040", "041", "042", "043",
    "044", "045", "046", "047",
    "048", "049", "050", "051",
    "052", "053", "054", "055",
    "056", "057", "058", "059",
    "060", "061", "062", "063",
    "064", "065", "066", "067",
    "068", "069", "070", "071",
    "072", "073", "074", "075",
    "076", "077", "078", "079",
    "080", "081", "082", "083",
    "084", "085", "086", "087",
    "088", "089", "090", "091",
    "092", "093", "094", "095",
    "096", "097", "098", "099",
    "100", "101", "102", "103",
    "104", "105", "106", "107",
    "108", "109", "110", "111",
    "112", "113", "114", "115",
    "116", "117", "118", "119",
    "120", "121", "122", "123",
    "124", "125", "126", "127",
    "128", "129", "130", "131",
    "132", "133", "134", "135",
    "136", "137", "138", "139",
    "140", "141", "142", "143",
    "144", "145", "146", "147",
    "148", "149", "150", "151",
    "152", "153", "154", "155",
    "156", "157", "158", "159",
    "160", "161", "162", "163",
    "164", "165", "166", "167",
    "168", "169", "170", "171",
    "172", "173", "174", "175",
    "176", "177", "178", "179",
    "180", 0
};
static const char *AID0[128] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", "011", "012",
    "013", "014", "015", "016",
    "017", "018", "019", "020",
    "021", "022", "023", "024",
    "025", "026", "027", "028",
    "029", "030", "031", "032",
    "033", "034", "035", "036",
    "037", "038", "039", "040",
    "041", "042", "043", "044",
    "045", "046", "047", "048",
    "049", "050", "051", "052",
    "053", "054", "055", "056",
    "057", "058", "059", "060",
    "061", "062", "063", "064",
    "065", "066", "067", "068",
    "069", "070", "071", "072",
    "073", "074", "075", "076",
    "077", "078", "079", "080",
    "081", "082", "083", "084",
    "085", "086", "087", "088",
    "089", "090", "091", "092",
    "093", "094", "095", "096",
    "097", "098", "099", "100",
    "101", "102", "103", "104",
    "105", "106", "107", "108",
    "109", "110", "111", "112",
    "113", "114", "115", "116",
    "117", "118", "119", "120",
    "121", "122", "123", "124",
    "125", "126", "127", 0
};
static const char *MU[131] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", "011", "012",
    "013", "014", "015", "016",
    "017", "018", "019", "020",
    "021", "022", "023", "024",
    "025", "026", "027", "028",
    "029", "030", "031", "032",
    "033", "034", "035", "036",
    "037", "038", "039", "040",
    "041", "042", "043", "044",
    "045", "046", "047", "048",
    "049", "050", "051", "052",
    "053", "054", "055", "056",
    "057", "058", "059", "060",
    "061", "062", "063", "064",
    "065", "066", "067", "068",
    "069", "070", "071", "072",
    "073", "074", "075", "076",
    "077", "078", "079", "080",
    "081", "082", "083", "084",
    "085", "086", "087", "088",
    "089", "090", "091", "092",
    "093", "094", "095", "096",
    "097", "098", "099", "100",
    "101", "102", "103", "104",
    "105", "106", "107", "108",
    "109", "110", "111", "112",
    "113", "114", "115", "116",
    "117", "118", "119", "120",
    "121", "122", "123", "124",
    "125", "126", "127", "128",
    "129", "130", 0
};
static const char *MUW[21] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", "011", "012",
    "013", "014", "015", "016",
    "017", "018", "019", "020",
    0
};
static const char *TRE[11] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", 0
};
static const char *MUX[11] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", 0
};
static const char *MUF[11] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", 0
};
static const char *MUS[11] = {
    "001", "002", "003", "004",
    "005", "006", "007", "008",
    "009", "010", 0
};
static ResKeyTable CID[8] = {
    {"", CID0, "pc\\"},
    {"", CID1, "npc\\"},
    {"", CID2, "ppl\\"},
    {"", CID3, "ppl\\"},
    {"", CID4, "ppl\\"},
    {"", CID5, "ppl\\"},
    {"", CID6, "citi\\"},
    {0, 0, 0}
};
static ResKeyTable RID[4] = {
    {"vx_", RID0, "vx\\vx_"},
    {"ag", RID1, "ag\\ag"},
    {"", RID2, "etc\\"},
    {0, 0, 0}
};
static ResKeyTable WID[10] = {
    {"kos_", WID0, "pc\\kos_"},
    {"mmo_", WID1, "pc\\mmo_"},
    {"zig_", WID2, "pc\\zig_"},
    {"jr_", WID3, "pc\\jr_"},
    {"stn_", WID4, "pc\\stn_"},
    {"son_", WID5, "pc\\son_"},
    {"", WID6, "robo\\"},
    {"", WID7, "enemy\\"},
    {"cho_", WID8, "pc\\cho_"},
    {0, 0, 0}
};
static ResKeyTable EID[17] = {
    {"guno_", EID0, "guno\\guno_"},
    {"utma_", EID1, "utma\\utma_"},
    {"utre_", EID2, "utre\\utre_"},
    {"utso_", EID3, "utso\\utso_"},
    {"utro_", EID4, "utro\\utro_"},
    {"fema_", EID5, "fema\\fema_"},
    {"fero_", EID6, "fero\\fero_"},
    {"feso_", EID7, "feso\\feso_"},
    {"fere_", EID8, "fere\\fere_"},
    {"_", EID8, "guno\\guno_"},
    {"_", EID8, "guno\\guno_"},
    {"_", EID8, "guno\\guno_"},
    {"_", EID8, "guno\\guno_"},
    {"_", EID8, "guno\\guno_"},
    {"_", EID8, "guno\\guno_"},
    {"boss_", EIDf, "boss\\boss_"},
    {0, 0, 0}
};
static ResKeyTable MID[2] = {
    {"", MID0, "mobj000\\mobj_"},
    {0, 0, 0}
};
static ResKeyTable AID[2] = {
    {"", AID0, "aobj000\\acc_"},
    {0, 0, 0}
};
static ResKeyTable MOBJ[10] = {
    {"mu_", MU, "muobj\\MU_"},
    {"muw_", MUW, "muwobj\\muw_"},
    {"tre_", TRE, "treobj\\tre_"},
    {"tre_", TRE, "treobj\\tre_"},
    {"mux_", MUX, "muxobj\\mux_"},
    {"mux_", MUX, "muxobj\\mux_"},
    {"muf_", MUF, "mufobj\\muf_"},
    {"muf_", MUF, "mufobj\\muf_"},
    {"mus_", MUS, "musobj\\mus_"},
    {0, 0, 0}
};
static ResKeyTable model[9] = {
    {"cid_", CID, "char\\"},
    {"cid_", CID, "char\\"},
    {"rid_", RID, "robo\\"},
    {"wid_", WID, "weapon\\"},
    {"eid_", EID, "enemy\\"},
    {"mid_", MID, "obj\\"},
    {"aid_", AID, "obj\\"},
    {"", MOBJ, "obj\\"},
    {0, 0, 0}
};
CfMtnEntry cfMtnList[31] = {
    {"shion_cf", {1, 9, 10, 11, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"kosmos_cf", {2, 14, 15, 16, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"chaos_cf", {3, 18, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"momo_cf", {4, 20, 21, 22, 23, 50, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"jr_cf", {5, 24, 25, 26, 47, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"ziggy_cf", {6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"shitan_cf", {7, 28, 29, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"allen_cf", {263, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"oldw_cf", {1579, 1582, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"oldm_cf", {1549, 1552, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"middlem_cf", {1543, 1546, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"middlew_cf", {1573, 1576, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"soldierw_cf", {1289, 542, 523, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"soldierm_cf", {526, 536, 525, 513, 527, 1288, 537, 519, 0, 0, 0, 0, 0, 0}},
    {"woman_cf", {1567, 1570, 783, 533, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"youngm_cf", {1537, 1540, 780, 530, 529, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"youngw_cf", {1591, 1594, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno001_cf", {16385, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_301", {16410, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_302", {16411, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_303", {16412, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_304", {16413, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_305", {16414, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_323", {16432, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_324", {16433, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_325", {16434, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_301", {16410, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_302", {16411, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_303", {16412, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"guno_307", {16416, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
};
static unsigned short facePC[54] = {
    0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1,
    0x1, 0x1, 0x2, 0x2, 0x2, 0x2, 0x3, 0x3, 0x4, 0x4, 0x4, 0x4,
    0x5, 0x5, 0x5, 0x6, 0x0, 0x0, 0x1E, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x1F, 0x1F, 0x1F, 0x1F, 0x25, 0x1F, 0x1E, 0x22, 0x1E, 0x1E, 0x5,
    0x1E, 0x1E, 0x4, 0x21, 0x4, 0x1E
};
static unsigned short faceNPC[89] = {
    0x0, 0x0, 0x102, 0x0, 0x5, 0x0, 0x5, 0x0, 0x107, 0x10A, 0x0, 0x10A,
    0x10D, 0x10D, 0x10D, 0x10D, 0x10D, 0x0, 0x0, 0x0, 0x113, 0x0, 0x0, 0x0,
    0x117, 0x117, 0x117, 0x117, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x137, 0x11E, 0x0, 0x0,
    0x0, 0x0, 0x144, 0x0, 0x0, 0x0, 0x0, 0x0, 0x144, 0x117, 0x117, 0x117,
    0x117, 0x13B, 0x144, 0x144, 0x14C, 0x14D, 0x14E, 0x14F, 0x150, 0x151, 0x152, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x14C
};
static unsigned short facePPL1[32] = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x205, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x301, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x219, 0x219, 0x219, 0x219, 0x219, 0x21E, 0x21E
};
static unsigned short facePPL2[29] = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x30F, 0x30F, 0x30F, 0x0, 0x0,
    0x0, 0x30C, 0x30C, 0x0, 0x30F, 0x30F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0
};
static unsigned short facePPL3[17] = {
    0x0, 0x0, 0x0, 0x202, 0x0, 0x304, 0x305, 0x0, 0x0, 0x0, 0x409, 0x409,
    0x409, 0x409, 0x135, 0x407, 0x304
};
static unsigned short facePPL4[10] = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};
FaceEnt faceTbl[7] = {
    {facePC, 54, 0},
    {faceNPC, 89, 256},
    {facePPL1, 32, 512},
    {facePPL2, 29, 768},
    {facePPL3, 17, 1024},
    {facePPL4, 10, 1280},
    {0, 0, 0}
};


static void default_callback(void)
{
    xglSleep();
}


const char *RES_getPath(void)
{
    static const char *tbl_0_00366420[20] __asm__("tbl.0") = {
        "data\\scene\\", "data\\scene\\test\\", "data\\scene\\check\\", "data\\scene\\cf\\",
        "data\\scene\\event\\", "data\\scene\\gash\\", "data\\scene\\fuji\\", "data\\scene\\kojima\\",
        "data\\scene\\sakisako\\", "data\\scene\\konishi\\", "data\\scene\\koji\\", "data\\scene\\nv-yone\\",
        "data\\scene\\nakahara\\", "data\\scene\\yajima\\", "data\\scene\\kuramoto\\", "data\\scene\\keiichi\\",
        "data\\scene\\sato\\", "data\\scene\\sugisawa\\", "data\\scene\\yagi\\", "data\\scene\\itai\\"
    };
    if (ScenePath >= 20)
        ScenePath = 0;
    return tbl_0_00366420[ScenePath];
}

static char *RES_getScenePath(char *path, int resource_id)
{
    const char *scene_path = RES_getPath();
    int scene_id;

    while ((*path = *scene_path) != 0) {
        path++;
        scene_path++;
    }

    if (resource_id <= 0x0fffffff) {
        path[0] = 'c';
        path[1] = 'f';
    } else {
        path[0] = 'e';
        path[1] = 'v';
    }

    scene_id = resource_id & 0x0fffffff;

    path[5] = (char)('0' + scene_id % 10);
    scene_id /= 10;
    path[4] = (char)('0' + scene_id % 10);
    scene_id /= 10;
    path[3] = (char)('0' + scene_id % 10);
    scene_id /= 10;
    path[2] = (char)('0' + scene_id % 10);
    path[6] = '.';
    path[7] = 't';
    path[8] = 'x';
    path[9] = 't';
    path[10] = 0;

    return path + 7;
}

int RES_GetEnemySeBank(int sound_id)
{
    int index = 0;
    int bank = 0x40000;

    for (; index < 8; index++) {
        if (EnemySeBank[index].id == sound_id)
            return bank;
        bank += 0x10000;
    }
    return -1;
}

int RES_GetEnemySeType(int sound_id)
{
    int index;

    for (index = 0; index < 8; index++) {
        if (EnemySeBank[index].id == sound_id)
            return EnemySeBank[index].type;
    }
    return -1;
}

int RES_GetEnemySeFoot(int sound_id)
{
    int index;

    for (index = 0; index < 8; index++) {
        if (EnemySeBank[index].id == sound_id)
            return EnemySeBank[index].foot;
    }
    return -1;
}

const char *RES_GetEnemySeName(int index)
{
    if (index < 8)
        return EnemySeBank[index].name;
    return 0;
}


static int RES_GetMotBaseID(int id)
{
    CfMtnEntry *p;
    u16 *q;
    int base;
    int ret;
    int cur;

    id &= 0xffff;
    for (p = cfMtnList; p->name != 0; p++) {
        q = p->ids;
        base = *q;
        for (; *q != 0; q++) {
            cur = *q;
            ret = base;
            if (cur == id)
                return ret;
        }
    }
    return 1;
}


static int convert_face(int id)
{
    FaceEnt *e;
    unsigned int idx = id;
    unsigned int key = idx & 0x7F00;
    unsigned short v;
    idx &= 0xFFFF;

    for (e = faceTbl; e->en != 0; e++) {
        if (e->key == key) {
            v = e->tbl[idx & 0xFF];
            if (v != 0)
                idx = v;
            break;
        }
    }
    return idx;
}

static int resource_typeid_translate(int arg0, int *arg1, int *arg2)
{
    int value;
    int upper;

    switch (*(unsigned short *)arg1) {
    case 0:
        value = *arg2;
        upper = value & 0xFFFF0000;
        if (upper == 0x10000) {
            *arg2 = convert_face(value & 0xFFFF) + upper;
            return 1;
        }
        break;
    case 2:
        if (*arg2 <= 0x02FFFFFF) {
            *arg2 = RES_GetMotBaseID(*arg2);
            return 1;
        }
        break;
    case 3:
        *arg1 = 2;
        *arg2 = arg0 | 0x03000000;
        return 1;
    case 5:
        *arg2 = arg0;
        return 1;
    }
    return 0;
}

/*
 * The runtime resource-block table: up to 128 blocks carved out of the
 * arc-file load heap GameResourceInit sizes. address/size describe the
 * block's extent; handle is a random token GameResourceAlloc
 * (main:0x0024a9d8) assigns with xglSRand() when it hands a block out, and
 * both it and GameResourceReset (main:0x0024d8e8) clear it back to 0; state
 * is -1 for a free block (this TU's readers) and 9 once GameResourceAlloc
 * marks one in use (original bytes; neither function is claimed here).
 */
typedef struct {
    unsigned char *address;   /* +0x0 */
    int size;       /* +0x4 */
    int handle;     /* +0x8 */
    int state;      /* +0xC, -1 = free */
} GameResourceEntry;

extern GameResourceEntry GameResource[128];

int GameResourceSearch(int type, int id)
{
    int i;
    int a;
    int b;

    if (type == 5) {
        return -1;
    }
    for (i = 0; i < 0x80; i++) {
        a = type;
        b = id;
        resource_typeid_translate(i, &a, &b);
        if (type == -1 || GameResource[i].state == a) {
            if (id == -1 || GameResource[i].handle == b) {
                return i;
            }
        }
    }
    return -1;
}

static int resource_get_free(void)
{
    int index;

    for (index = 0; index < 0x80; index++) {
        if (GameResource[index].state == -1)
            return index;
    }
    return -1;
}

unsigned int GameResourceGetFreeAddr(void)
{
    unsigned int a = GameResource[resource_get_free()].address;
    if (a < 0x112A000) a = 0x112A000;
    return a;
}



int GameResourceWorkAlloc(int size)
{
    int *p;
    int i;

    GameResourceWorkReloadLeader = 0;
    GameResourceWorkReloadTable[0] = 0;
    p = &GameResource[0].address;
    for (i = 0; i < 0x80; i++, p += 4) {
        if (p[3] == -1) {
            if (size < p[1])
                return p[0];
            goto fail;
        }
    }
fail:
    GameResourceWorkReloadTable[0] = 1;
    return 0x112A000;
}

/*
 * The game loop's own record, as far as this TU reads it. Two spans are
 * evidenced: the runtime flags word command_player_lock ORs a bit into
 * (lw/ori/sw +0x10, main:0x0024c188) and the scene id GameResourceWorkReload
 * reloads by (lw +0x18, main:0x0024a998). Nothing else is modelled; the
 * object is still owned by the generated .sdata scaffolding.
 */
typedef struct {
    u8 unmodeled_00[0x10];
    u32 flags;      /* +0x10 */
    u8 unmodeled_14[4];
    int scene_id;   /* +0x18 */
} GameLoopStatePrefix;

extern GameLoopStatePrefix GameLoopState;
extern void ACT_init(void);
extern void MapChangeFadeSet(void);
extern void GameResourceLoad(int scene_id);

void GameResourceWorkReload(void)
{
    if (GameResourceWorkReloadTable[0] != 0) {
        ACT_init();
        GameResourceLoad(GameLoopState.scene_id);
        return;
    }
    MapChangeFadeSet();
}

/* Types ported from the derwetterfrosch fork (2026-10-19). */
typedef struct {
    const char *name;
    u16 ids[14];
} MotionEntry;

typedef struct {
    u16 *table;
    u16 count;
    u16 group;
} FaceEntry;

void *GameResourceAlloc(int size)
{
    int index;

    index = resource_get_free();
    if (index == -1)
        return 0;

    size = (size + 63) & ~63;
    GameResource[index + 1].address = GameResource[index].address + size;
    GameResource[index + 1].size = GameResource[index].size - size;
    GameResource[index].size = size;
    GameResource[index].state = 9;
    GameResource[index].handle = xglSRand();
    return GameResource[index].address;
}

extern int GameResourceGetIndex(int id);

int GameResourceRealloc(int resource, int size)
{
    int i = GameResourceGetIndex(resource);
    int total;
    u32 addr;
    if (i < 0) {
        return 0;
    }
    if (GameResource[i + 1].state == -1) {
        total = GameResource[i].size;
        size = (size + 0x3f) & ~0x3f;
        total = total + GameResource[i + 1].size;
        addr = GameResource[i].address;
        GameResource[i].size = size;
        GameResource[i + 1].address = addr + size;
        GameResource[i + 1].size = total - size;
        return resource;
    }
    return 0;
}

extern int xglArxExtract(void *destination, void *source);
extern void *memcpy(void *destination, const void *source, unsigned int count);

static int next_arc_size(void *address)
{
    u32 *src;
    int size;

    if (arcno == 0) {
        u32 *s = arcfileaddr;
        u32 *d = archeader;
        int i;

        for (i = *arcfileaddr >> 2; i >= 0; i--) {
            *d++ = *s++;
        }
        src = (u32 *)((char *)arcfileaddr + ((archeader[0] + 0x40) & ~0x3F));
        size = archeader[1];
    } else {
        size = archeader[arcno + 1] - archeader[arcno];
        src = (u32 *)((char *)arcfileaddr + ((archeader[0] + 0x40) & ~0x3F) + archeader[arcno]);
    }
    arcno++;
    if (size > 0) {
        if (*src == 0x585241) {
            size = xglArxExtract(address, src);
        } else {
            memcpy(address, src, size);
        }
    }
    if (arcno >= archeader[0] >> 2) {
        arcfileaddr = 0;
    }
    return size;
}

static u8 *next_line(u8 *cursor)
{
    do {
        while (*cursor != '\n' && *cursor != '\r')
            cursor++;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
            cursor++;
    } while (*cursor == ';');
    return cursor;
}

static u8 *skip_space(u8 *cursor)
{
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    return cursor;
}

static char *search_key(char *cursor, u8 *table, int *id)
{
    ResourceKeyEntry *var_22;
    ResourceKeyEntry *var_15;
    char **var_11;
    char *var_3;
    char *var_3_2;
    char *var_3_3;
    char *var_6;
    char *var_21;
    char *var_24;
    char *var_13;
    char *var_9;
    char *var_10;
    char *temp_2_3;
    char *temp_2_2;
    char *temp_2;
    char *sp0;
    int *sp4;
    int var_30;
    int var_25;
    int var_14;
    int var_2;
    signed char var_4;
    signed char var_12;
    signed char var_3_4;
    signed char var_3_5;
    signed char temp_7;
    signed char var_23;
    signed char var_5;

    var_22 = (ResourceKeyEntry *)table;
    var_30 = 0;
    sp0 = cursor;
    var_3 = (char *)var_22->key;
    sp4 = id;
    if (var_3 != 0) {
loop_1:
        var_6 = sp0;
        var_21 = var_3;
loop_2:
        var_4 = *var_21;
        var_23 = *var_6;
        if ((var_4 == 0) && (var_15 = (ResourceKeyEntry *)var_22->aliases, var_3_2 = (char *)var_15->key, var_25 = var_30, (var_3_2 != 0))) {
            for (;;) {
                var_13 = var_6;
                var_24 = var_3_2;
loop_6:
                var_12 = *var_24;
                var_5 = *var_13;
                if ((var_12 == 0) && (var_11 = (char **)var_15->aliases, var_3_3 = *var_11, var_14 = var_25 + 1, (var_3_3 != 0))) {
                    int k_plus = 0x2B;
                    int k_sp = 0x20;
                    int k_tab = 9;
                    int k_nl = 10;
                    int k_cr = 13;
loop_9:
                    var_10 = var_13;
                    var_9 = var_3_3;
loop_10:
                    temp_7 = *var_10;
                    var_3_4 = *var_9;
                    if (var_3_4 == k_plus) {
                        var_9 += 1;
                        var_3_4 = *var_9;
                    }
                    if (var_3_4 == 0x2C) {
                        var_3_4 = 0;
                    }
                    var_3_5 = var_3_4;
                    if (var_3_5 == 0 && (temp_7 == 0x2C || temp_7 == k_sp || temp_7 == k_tab || temp_7 == k_nl || temp_7 == k_cr)) {
                        *sp4 = var_14;
                        return var_10;
                    }
                    var_2 = var_3_5 - 0x61;
block_23:
                    if ((u32)(var_2 & 0xFF) < 0x1AU) {
                        var_3_5 -= 0x20;
                    }
                    if (temp_7 == var_3_5) {
                        var_10 += 1;
                        var_9 += 1;
                        goto loop_10;
                    }
                    var_11 += 1;
                    var_14 += 1;
                    temp_2 = *var_11;
                    var_3_3 = temp_2;
                    if (temp_2 == 0) {
                        goto block_31;
                    }
                    goto loop_9;
                } else {
block_31:
                    if ((u32)(var_12 - 0x61) < 0x1AU) {
                        var_12 -= 0x20;
                    }
                    if (var_5 == var_12) {
                        var_13 += 1;
                        var_24 += 1;
                        goto loop_6;
                    }
                    var_15 += 1;
                    var_25 += 0x100;
                    temp_2_2 = (char *)var_15->key;
                    var_3_2 = temp_2_2;
                    if (temp_2_2 == 0) {
                        goto block_39;
                    }
                }
            }
        } else {
block_39:
            if ((u32)(var_4 - 0x61) < 0x1AU) {
                var_4 -= 0x20;
            }
            if (var_23 == var_4) {
                var_6 += 1;
                var_21 += 1;
                goto loop_2;
            }
            var_22 += 1;
            var_30 += 0x1000;
            temp_2_3 = (char *)var_22->key;
            var_3 = temp_2_3;
            if (temp_2_3 == 0) {
                goto block_47;
            }
            goto loop_1;
        }
    } else {
block_47:
        *sp4 = 0;
        return sp0;
    }
}

extern void GameResourceReset(int reset_mode);

static unsigned char *command_reset(int command, unsigned char *cursor, int mode)
{
    int n = 0;

    while ((unsigned char)(*cursor - '0') < 10) {
        int c = *cursor++;
        n *= 10;
        n = n + c - '0';
    }
    GameResourceReset(n);
    return cursor;
}

typedef struct {
    char **names;
    int count;
} LoadMapNameBlock;
extern LoadMapNameBlock maptbl[];
extern void MapReset(void);
typedef struct {
    u8 pad[0x50];
    short map_id;
    short pad52;
    int map_res;
} LoadMapLoopView;

static unsigned char *command_loadmap(int command, unsigned char *cursor, int mode)
{
  int keep = 0;
  LoadMapNameBlock *tbl;
  int id;
  int i;
  int r;
  char *s;
  char *c;
  unsigned short new_var;
  new_var = '-';
  if ((*cursor) == new_var)
  {
    if (tbl || s)
    {
      cursor++;
      keep = 1;
    }
    else
    {
      cursor++;
      keep = 1;
    }
  }
  tbl = maptbl;
  id = 0;
  do
  {
    for (; tbl->names != 0; tbl++, id += 100)
    {
      char **pp = tbl->names;
      for (i = 0; i < tbl->count; i++, pp++)
      {
        s = *pp;
        c = (char *) cursor;
        for (;;)
        {
          if ((*s) == 0)
          {
            if (((((*c) == ' ') || ((*c) == '\t')) || ((*c) == '\n')) || ((*c) == '\r'))
            {
              cursor = (unsigned char *) c;
              id += i;
              goto found;
            }
          }
          if ((*(c++)) != (*(s++)))
          {
            break;
          }
        }

        if (!i)
        {
        }
      }

    }

    if (id >= 0x71E)
    {
      id = 0;
    }
    found:
    r = RES_loadFile(command, 1, id, 0);

  }
  while (0);
  if (keep == 0)
  {
    LoadMapLoopView *g = (LoadMapLoopView *) (&GameLoopState);
 do { g->map_res = r; g->map_id = id; MapChangeFadeSet(); MapReset(); } while (0);
  }
  return cursor;
}

extern int RES_loadFile(int command, int callback, int resource_id, int flags);
static char *search_key(char *cursor, u8 *table, int *id);

static char *command_loadact(int command, char *cursor, int mode)
{
    int resource_id = 0;
    char *next_cursor = search_key(cursor, (u8 *)model, &resource_id);

    RES_loadFile(command, 0, resource_id, 0);
    return next_cursor;
}

static char *command_loadface(int command, char *cursor, int mode)
{
    int resource_id = 0;
    char *next_cursor = search_key(cursor, (u8 *)model, &resource_id);

    RES_loadFile(command, 0, resource_id + 0x10000, 0);
    return next_cursor;
}

/*
 * The resource record an actor's `resource` pointer refers to.
 * RES_GetFootStepNo is the only reader in this TU, and it reads only the
 * footstep number byte at +6; the bytes before it are an unread span here.
 */
typedef struct ActorResource {
    unsigned char unmodeled_000[6];
    u8 foot_step_no; /* +6 */
} ActorResource;

static char *command_loadmot(int command, char *cursor)
{
    char name[0x100];
    char *dst = name;
    int character;
    int motion_id;
    int first;
    short value;
    int foot_step;

    character = (u8)*cursor;
    value = character;
    while ((character & 0xff) != ',' && (character & 0xff) != ' ' &&
           (character & 0xff) != '\t' && (character & 0xff) != '\n' &&
           (character & 0xff) != '\r') {
        *dst = value;
        dst++;
        cursor++;
        character = (u8)*cursor;
        value = character;
    }
    *dst = '\0';

    for (;;) {
        first = (u8)character;
        if (first == ',' || first == ' ' || first == '\t') {
            cursor++;
            character = (u8)*cursor;
            continue;
        }
        break;
    }

    motion_id = 0;
    while ((u8)((u8)character + 0xD0) < 10U) {
        int digit;
        int parsed_id;

        digit = (u8)*cursor;
        cursor++;
        motion_id = motion_id * 10;
        parsed_id = motion_id + digit;
        motion_id = parsed_id - '0';
        character = *cursor & 0xff;
    }

    foot_step = motion_id;
    motion_id = 0;
    {
        ActorResource *resource = RES_loadFile(command, 3, (int)name, 0);

        resource->foot_step_no = foot_step;
    }
    return cursor;
}

/*
 * The engine's actor record: main/tu194 (src/main/enemy_2.h) defines and
 * owns it. This TU only reads the resource pointer past that definition's
 * extent, so it names the same tag without completing it (no local header
 * can include another TU's TU-local one) rather than modelling a
 * competing layout.
 */
struct Actor;

/*
 * +0x8dc: the actor's resource pointer, past the extent src/main/enemy_2.h
 * completes; RES_GetFootStepNo is the only reader of it in this TU.
 */
#define ACTOR_RESOURCE_OFFSET 0x8dc
#define ACTOR_RESOURCE(actor) \
    (*(ActorResource **)((unsigned char *)(actor) + ACTOR_RESOURCE_OFFSET))

u8 RES_GetFootStepNo(struct Actor *actor)
{
    ActorResource *resource = ACTOR_RESOURCE(actor);
    u8 foot_step_no = 0;

    if (resource != 0) {
        foot_step_no = resource->foot_step_no;
    }
    return foot_step_no;
}

static unsigned char *command_loadtex(int command, unsigned char *cursor, int mode)
{
    int n = 0;

    while ((unsigned char)(*cursor - '0') < 10) {
        int c = *cursor++;
        n *= 10;
        n = n + c - '0';
    }
    RES_loadFile(command, 11, n, 0);
    return cursor;
}

static unsigned char *command_loadmovie(int command, unsigned char *cursor, int mode)
{
    int n = 0;

    while ((unsigned char)(*cursor - '0') < 10) {
        int c = *cursor++;
        n *= 10;
        n = n + c - '0';
    }
    RES_loadFile(command, 10, n, 0);
    return cursor;
}

extern int sefLoadEffectCfName(int cf_id, int effect_name);
extern int sefLoadMemoryEffectCfName(int cf_id, int effect_name, int buffer);

static void command_loadeffect_sub(int type, int effect_name, void *address)
{
    if (arcfileaddr != 0) {
        sefLoadMemoryEffectCfName((int)address, effect_name, next_arc_size(address));
        return;
    }
    sefLoadEffectCfName((int)address, effect_name);
}

static unsigned char *command_loadeffect(int command, unsigned char *cursor, int mode)
{
    char name[256];
    char *p = name;

    while (*cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
        *p++ = *cursor++;
    }
    *p = 0;
    RES_loadFile(command, 6, (int)name, (int)command_loadeffect_sub);
    return cursor;
}

/* RES_loadFile callback: (resource type, file name, resource address). */
static int command_loadene_sub(int type, const char *name, void *address)
{
    if (arcfileaddr != 0)
        next_arc_size(AdrsEnemyPreset);
    else
        Enemy_LoadPreset(AdrsEnemyPreset, name);

    return 0;
}

static unsigned char *command_loadene(int command, unsigned char *cursor, int mode)
{
    char name[256];
    char *p = name;

    while (*cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
        *p++ = *cursor++;
    }
    *p = 0;
    RES_loadFile(command, 8, (int)name, (int)command_loadene_sub);
    return cursor;
}

extern int SCRIPT_load2(const char *path);
extern void SCRIPT_exec2(void);

static unsigned char *command_script(int command, unsigned char *cursor, int mode)
{
    char name[256];
    char *p = name;
    const char *src = RES_getPath();

    while ((*p = *src) != 0) {
        src++;
        p++;
    }
    while (*cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
        *p++ = *cursor++;
    }
    p[0] = '.';
    p[1] = 'e';
    p[2] = 'v';
    p[3] = 't';
    p[4] = 0;
    if (!(GameLoopState.flags & 0x40)) {
        SCRIPT_load2(name);
        SCRIPT_exec2();
    }
    return cursor;
}

extern void GameCfPlayerLoadResource(int resource_set);

static char *command_player(int command, char *cursor, int mode)
{
    GameCfPlayerLoadResource(0);
    return cursor;
}

extern void *GameResourceAlloc(int size);

extern int GameResourceRealloc(int resource, int size);

static char *command_dummy(int command, char *cursor, int mode)
{
    if (GameResource[command + 1].address == 0) {
        ((void *(*)(int, int))GameResourceAlloc)(0, 0);
    } else {
        ((int (*)())GameResourceRealloc)(GameResource[command].address);
    }
    return cursor;
}

extern unsigned char SaveData[];

void RES_GetLeaderSeName(char *dst)
{
    static char *tbl_1_00366470[7] __asm__("tbl.1") = {
        (char *)"", (char *)"SO", (char *)"KM", (char *)"CS", (char *)"MO", (char *)"JR", (char *)"Z8"
    };
    char *src = tbl_1_00366470[*(u16 *)(SaveData + 0x100A4)];
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = '_';
    dst += 3;
    src = foot_name;
    while ((*dst = *src) != 0) {
        src++;
        dst++;
    }
}


void RES_GetMapEnvSeName(char *name)
{
    char *env = env_name;

    while ((*name = *env) != 0) {
        env++;
        name++;
    }
}

#include "main/xgl_sound.h"
extern struct SoundWork SoundWork;
int sprintf(char *buffer, const char *format, ...);
extern int SsdGetResultValue(int *result);
extern int SsdGetSeqPlayStatus(int sequence_id);
extern int SsdCheckPlayEffectFileID(u16 file_id);
extern void xglSoundSendSmd(void *address);
extern void xglSoundSequenceNormal(int mode);
typedef struct {
    u8 unmodeled_00[14];
    u16 sequence_id;
} SoundLoadBuffer;

static u8 *command_loadse(int command, u8 *cursor)
{
    char sound_name[0x40];
    char sequence_name[0x40];
    char path[0x100];
    int character;
    u32 address;
    u16 file_id;
    int keep_current;
    u8 set_normal;
    int first_size;
    int second_size;
    u8 *name_end;
    u8 *sequence_end;
    void *second_address;

    address = GameResourceGetFreeAddr();
    name_end = sound_name;
    for (;;) {
        character = *cursor;
        if ((character & 0xff) == ',' || (character & 0xff) == ' ' ||
            (character & 0xff) == '\t' || (character & 0xff) == '\n' ||
            (character & 0xff) == '\r')
            break;
        *name_end++ = character;
        cursor++;
    }
    *name_end = 0;

    keep_current = 0;
    for (;;) {
        character = (u8)*cursor;
        if ((character & 0xff) == ',' || (character & 0xff) == ' ' ||
            (character & 0xff) == '\t') {
            cursor++;
            continue;
        }
        break;
    }
    name_end = env_name;
    goto environment_name_test;

environment_name_copy:
    if (character == '-') {
        keep_current = 1;
        cursor++;
        goto environment_name_done;
    }
    *name_end++ = character;
    character = *++cursor;

environment_name_test:
    if ((u8)character != ',' && (u8)character != ' ' &&
        (u8)character != '\t' && (u8)character != '\n' &&
        (u8)character != '\r')
        goto environment_name_copy;

environment_name_done:
    *name_end = 0;

    set_normal = 0;
    for (;;) {
        character = (u8)*cursor;
        if ((character & 0xff) == ',' || (character & 0xff) == ' ' ||
            (character & 0xff) == '\t') {
            cursor++;
            continue;
        }
        break;
    }
    sequence_end = sequence_name;
    while ((u8)character != ',' && (u8)character != ' ' &&
           (u8)character != '\t' && (u8)character != '\n' &&
           (u8)character != '\r') {
        if ((u8)character == '+') {
            set_normal = 1;
            break;
        }
        *sequence_end++ = character;
        character = *++cursor;
    }
    *sequence_end = 0;

    if (sound_name[0] == 0) {
        xglSoundLoadEffect(0, 0, 2);
    } else {
        const char *src;
        char *dst = foot_name;
        if (sound_name[0] == 'S' && sound_name[1] == 'O' && sound_name[2] == '_')
            src = sound_name + 3;
        else
            src = sound_name;
        while ((*dst = *src) != 0) {
            src++;
            dst++;
        }
    }

    if (env_name[0] == 0) {
        xglSoundLoadEffect(0, 0, 3);
    } else {
        file_id = SoundWork.effect_banks[3].handle;
        ((SoundLoadBuffer *)address)->sequence_id = file_id;
        if (arcfileaddr != 0) {
            first_size = next_arc_size((void *)address);
            second_address = (void *)(address + first_size);
            second_size = next_arc_size(second_address);
        } else {
            sprintf(path, "data\\sound\\sed\\%s.SWD", env_name);
            first_size = xglCdReadFile(path, (void *)address, 0, 0);
            second_address = (void *)(address + first_size);
            sprintf(path, "data\\sound\\sed\\%s.SED", env_name);
            second_size = xglCdReadFile(path, second_address, 0, 0);
        }
        if (file_id != ((SoundLoadBuffer *)address)->sequence_id) {
            keep_current = 0;
        } else {
            int request_status;
            int effect_result;
            int sequence_result;
            SsdCheckPlayEffectFileID(file_id);
            do { request_status = SsdGetResultValue(&effect_result); } while (request_status < 0);
            SsdGetSeqPlayStatus(file_id);
            do { request_status = SsdGetResultValue(&sequence_result); } while (request_status < 0);
            if (effect_result == 0) {
                if (sequence_result != 1)
                    keep_current = 0;
            }
        }
        if (!keep_current && first_size > 0 && second_size > 0)
            xglSoundSendEffect((void *)address, second_address, 3);
    }

    if (sequence_name[0] != 0) {
        if (sequence_name[0] == '.') {
            xglSoundSendSmd(0);
        } else {
            int size;
            if (arcfileaddr != 0) {
                size = next_arc_size((void *)address);
            } else {
                sprintf(path, "data\\sound\\smd\\%s.SMD", sequence_name);
                size = xglCdReadFile(path, (void *)address, 0, 0);
            }
            if (size != 0) {
                xglSoundSendSmd((void *)address);
                if (set_normal)
                    xglSoundSequenceNormal(0x7F);
            }
        }
    }
    return cursor;
}

extern int xglSoundSendSmd2(void *smd, int bank);
extern void xglSoundSequenceNormal2(int channel, int volume);

static u8 *command_loadsmd(int command, u8 *cursor)
{
    char name[0x40];
    char path[0x100];
    s16 character;
    u32 address;
    int sequence_index;
    int first_character;

    sequence_index = 0;
    address = GameResourceGetFreeAddr();
    first_character = *cursor;
    if ((u8)first_character != '\n' && (u8)first_character != '\r') {
        int separator_value;

        do {
            char *name_end;
            int stop_current;
            int set_normal;
            int file_size;
            int request_status;
            int sequence_status;
            u16 sequence_id;

            character = *cursor;
            set_normal = 0;
            stop_current = 0;
            name_end = name;
            if ((u8)character != ',' && (u8)character != ' ' &&
                (u8)character != '\t' && (u8)character != '\n' &&
                (u8)character != '\r') {
                for (;;) {
                    if ((character & 0xff) == '+') {
                        cursor++;
                        set_normal = 1;
                    } else if ((character & 0xff) == '-') {
                        cursor++;
                        stop_current = 1;
                    } else {
                        *name_end++ = character;
                        cursor++;
                    }
                    character = *cursor;
                    if ((u8)character == ',' || (u8)character == ' ' ||
                        (u8)character == '\t' || (u8)character == '\n' ||
                        (u8)character == '\r')
                        break;
                }
            }

            sequence_id = SoundWork.channels[sequence_index].sequence;
            *name_end = '\0';
            ((SoundLoadBuffer *)address)->sequence_id = sequence_id;
            if (arcfileaddr != 0) {
                file_size = next_arc_size((void *)address);
            } else {
                const char *sequence_format = "data\\sound\\smd\\%s.SMD";

                sprintf(path, sequence_format, name);
                file_size = xglCdReadFile(path, (void *)address, 0, 0);
            }

            if (sequence_id != ((SoundLoadBuffer *)address)->sequence_id) {
                stop_current = 0;
            } else {
                SsdGetSeqPlayStatus(sequence_id);
                do {
                    request_status = SsdGetResultValue(&sequence_status);
                } while (request_status < 0);
                if (sequence_status != 1)
                    stop_current = 0;
            }
            if (!stop_current && file_size > 0) {
                xglSoundSendSmd2((void *)address, sequence_index);
                if (set_normal)
                    xglSoundSequenceNormal2(sequence_index, 0x7F);
            }

            for (;;) {
                first_character = *cursor;
                separator_value = first_character & 0xff;
                if (separator_value == ',' || separator_value == ' ' ||
                    separator_value == '\t') {
                    cursor++;
                } else {
                    break;
                }
            }
            sequence_index++;
        } while (separator_value != '\n' && separator_value != '\r');
    }
    return cursor;
}

extern void GameMpeg2Play(char *name, int mode);

static char *command_mpeg2_core(int command, char *cursor, int mode)
{
    static char base_2[] __asm__("base.2") = "data\\movie\\mpeg2\\";
    char path[256];
    char *s = base_2;
    char *d = path;
    int c;

    while ((*d = *s) != 0) {
        s++;
        d++;
    }
    c = *(unsigned char *)cursor;
    while ((unsigned char)c != ' ' && (unsigned char)c != '\t' && (unsigned char)c != '\n' && (unsigned char)c != '\r') {
        *d = c;
        cursor++;
        c = *(unsigned char *)cursor;
        d++;
    }
    d[2] = 's';
    d[0] = '.';
    d[1] = 'p';
    d[4] = '\0';
    d[3] = 's';
    GameMpeg2Play(path, mode);
    return cursor;
}

static char *command_mpeg2(int command, char *cursor)
{
    return command_mpeg2_core(command, cursor, 2);
}

static char *command_mpeg2battle(int command, char *cursor, int mode)
{
    SCRIPT_fade(2);
    return command_mpeg2_core(command, cursor, 1);
}

static char *command_mpeg2nofade(int command, char *cursor)
{
    return command_mpeg2_core(command, cursor, 5);
}

extern void SCRIPT_frameLock2Battle(void);

static char *command_event2battle(int command, char *cursor, int mode)
{
    SCRIPT_frameLock2Battle();
    return cursor;
}

static char *command_player_lock(int command, char *cursor, int mode)
{
    GameLoopState.flags |= 0x8000;
    return cursor;
}

extern char *strcpy(char *destination, const char *source);
extern void xglSoundLoadEffect(const char *name, void *address, int bank);
extern void xglSoundSendEffect(void *swd, void *sed, int bank);

static char *command_enenpcse(int command, char *cursor, int mode)
{
    char name[64];
    int id = 0;
    unsigned char *p;
    char *dst;
    int foot;
    int i;
    char *addr;
    char *ptr;
    int size;
    int size2;
    unsigned char k_comma = ',';
    unsigned char k_space = ' ';
    unsigned char k_tab = 9;
    unsigned char s_space;
    unsigned char s_comma;
    unsigned char s_tab;

    p = (unsigned char *)search_key(cursor, (u8 *)model, &id);

skip_top:
    if ((*p & 0xFF) == k_comma)
        goto skip_inc;
    if ((*p & 0xFF) == k_space)
        goto skip_inc;
    dst = name;
    if ((*p & 0xFF) != k_tab)
        goto skip_done;
skip_inc:
    p++;
    goto skip_top;
skip_done:
    for (;;) {
        if ((unsigned char)*p == ',' || (unsigned char)*p == ' ' || (unsigned char)*p == '\t' || (unsigned char)*p == '\n' || (unsigned char)*p == '\r')
            break;
        *dst++ = *p;
        p++;
    }
    *dst = 0;

    s_comma = ',';
    s_space = ' ';
    s_tab = 9;
    do {
skip2_top:
        if ((*p & 0xFF) == s_comma)
            goto skip2_inc;
        if ((*p & 0xFF) == s_space)
            goto skip2_inc;
        foot = 0;
        if ((*p & 0xFF) != s_tab)
            goto skip2_done;
skip2_inc:
        p++;
        goto skip2_top;
skip2_done:;
    } while (0);
    while ((unsigned char)(*p - '0') < 10) {
        int c = *p++;
        foot *= 10;
        foot = foot + c - '0';
    }

    addr = (char *)GameResourceGetFreeAddr();
    for (i = 0; i < 7 && EnemySeBank[i].id != 0; i++)
        ;
    EnemySeBank[i].id = id;
    EnemySeBank[i].type = mode;
    EnemySeBank[i].foot = foot;
    strcpy(EnemySeBank[i].name, name);

    if (arcfileaddr != 0) {
        size = next_arc_size(addr);
        ptr = addr + size;
        size2 = next_arc_size(ptr);
        if (size != 0 && size2 != 0)
            xglSoundSendEffect(addr, ptr, i + 4);
    } else {
        xglSoundLoadEffect(name, addr, i + 4);
    }
    return (char *)p;
}

static char *command_enese(int command, char *cursor, int mode)
{
    return command_enenpcse(command, cursor, 0);
}

static char *command_npcse(int command, char *cursor, int mode)
{
    return command_enenpcse(command, cursor, 1);
}

static char *command_disk(int command, char *cursor, int mode)
{
    GameDiskChange((unsigned char)cursor[0] - '0');
    return cursor;
}

static unsigned char *command_save(int command, unsigned char *cursor, int mode)
{
    int n = 0;

    while ((unsigned char)(*cursor - '0') < 10) {
        int c = *cursor++;
        n *= 10;
        n = n + c - '0';
    }
    Intermission(n);
    return cursor;
}

void GameResourcePreLoad(int scene_number)
{
    char path[256];
    char *archive_path;
    int archive_size;

    archive_path = RES_getScenePath(path, scene_number);
    arcfilepreload = 0;
    if (xglCdGetFileSize(path) > 0) {
        xglCdReadFile(path, scene_txt_buffer, 1, 1);

        archive_path[0] = 'a';
        archive_path[1] = '\0';
        archive_size = xglCdGetFileSize(path);
        if (archive_size > 0) {
            arcfilepreload = 0x02000000 - ((archive_size + 2047) & -2048);
            xglCdReadFile(path, (void *)arcfilepreload, 1, 1);
        }
    }
}

static void readreq(const char *name, void *buffer)
{
    xglCdReadFile(name, buffer, 0, 1);
}

typedef struct {
    const char *name;
    char *(*callback)(int, char *);
    void *unmodeled_08;
} GameResourceCommand;
static GameResourceCommand command[24] = {
    {"reset", (char *(*)(int, char *))command_reset, 0},
    {"loadmap", (char *(*)(int, char *))command_loadmap, 0},
    {"loadact", (char *(*)(int, char *))command_loadact, 0},
    {"loadface", (char *(*)(int, char *))command_loadface, 0},
    {"loadmot", (char *(*)(int, char *))command_loadmot, 0},
    {"loadtex", (char *(*)(int, char *))command_loadtex, 0},
    {"loadmovie", (char *(*)(int, char *))command_loadmovie, 0},
    {"loadeffect", (char *(*)(int, char *))command_loadeffect, 0},
    {"loadene", (char *(*)(int, char *))command_loadene, 0},
    {"script", (char *(*)(int, char *))command_script, 0},
    {"player", (char *(*)(int, char *))command_player, 0},
    {"dummy", (char *(*)(int, char *))command_dummy, 0},
    {"loadse", (char *(*)(int, char *))command_loadse, 0},
    {"loadsmd", (char *(*)(int, char *))command_loadsmd, 0},
    {"mpeg2", (char *(*)(int, char *))command_mpeg2, 0},
    {"mpeg2_btl", (char *(*)(int, char *))command_mpeg2battle, 0},
    {"mpeg2_nofade", (char *(*)(int, char *))command_mpeg2nofade, 0},
    {"enese", (char *(*)(int, char *))command_enese, 0},
    {"npcse", (char *(*)(int, char *))command_npcse, 0},
    {"event_btl", (char *(*)(int, char *))command_event2battle, 0},
    {"disk", (char *(*)(int, char *))command_disk, 0},
    {"save", (char *(*)(int, char *))command_save, 0},
    {"player_lock", (char *(*)(int, char *))command_player_lock, 0},
    {0, 0, 0}
};

void GameResourceLoad(int scene_id)
{
    typedef struct {
        u32 signature;
    } SceneTextHeader;

    char path[0x100];
    u8 *cursor;
    u8 *load_buffer;
    char *extension;
    int file_size;

    arcno = 0;
    GameLoopState.scene_id = scene_id;
    if (arcfilepreload != 0) {
        arcfileaddr = (u32 *)arcfilepreload;
    } else {
        load_buffer = (u8 *)scene_txt_buffer;
        ((SceneTextHeader *)load_buffer)->signature = 0xCDCDCDCD;
        arcfileaddr = 0;
        extension = RES_getScenePath(path, scene_id);
        readreq(path, load_buffer);
        *extension++ = 'a';
        *extension = '\0';
        file_size = xglCdGetFileSize(path);
        if (file_size > 0) {
            arcfileaddr = (u32 *)(0x02000000 - ((file_size + 0x7FF) & ~0x7FF));
            xglCdReadFile(path, arcfileaddr, 0, 1);
        }
    }

    cursor = (u8 *)scene_txt_buffer;
    if (((SceneTextHeader *)cursor)->signature == 0x6E637323) {
        int resource_id;

        cursor = next_line(cursor);
        for (;;) {
            GameResourceCommand *entry;

            if (*cursor == '#') {
                if (cursor[1] == 'e' && cursor[2] == 'n' && cursor[3] == 'd')
                    break;
                continue;
            } else {
                resource_id = 0;
                cursor = skip_space(cursor);
                if ((unsigned char)((unsigned char)*cursor + 0xD0) < 10) {
                    do {
                        resource_id *= 10;
                        resource_id += (u8)*cursor++ - '0';
                    } while ((unsigned char)((unsigned char)*cursor + 0xD0) < 10);
                }
                cursor = skip_space(cursor);
                entry = command;
                {
                int matched = 0;
                while (entry->name != 0) {
                    const char *name = entry->name;
                    u8 *scan = cursor;
                    char name_character;

                    for (;;) {
                        u8 name_byte;

                        name_character = *name;
                        name_byte = (u8)*name++;
                        if (name_character == '\0' &&
                            ((char)*scan == ' ' || (char)*scan == '\t' ||
                             (char)*scan == '\n' || (char)*scan == '\r')) {
                            cursor = scan;
                            matched = 1;
                            break;
                        }
                        if (*scan != name_byte)
                            break;
                        scan++;
                    }
                    if (matched)
                        break;
                    entry++;
                }
                if (matched) {
                    cursor = skip_space(cursor);
                    cursor = (u8 *)entry->callback(resource_id, (char *)cursor);
                }
                }
            }
            cursor = next_line(cursor);
        }
        arcfileaddr = 0;
        arcfilepreload = 0;
    }
}

extern char *MAP_getPath(void);

static int RES_loadFileSubMapSub(char *dir, char *name, void *buffer)
{
    static char dummy_3[] __asm__("dummy.3") = "data\\map\\dummy";
    char path[256];
    char *d = path;
    char *s;
    char *n;
    char *r;
    char *q;
    int size;

    q = MAP_getPath();
    while (*q != 0) *d++ = *q++;
    q = dir;
    while (*q != 0) *d++ = *q++;
    q = name;
    if (*q != 0) {
        do {
            *d++ = *(unsigned char *)q++;
        } while (*(unsigned char *)q != 0);
    }
    *d = 0;
    size = xglCdGetFileSize(path);
    if (size <= 0) {
        char *e = path;
        s = dummy_3;
        while (*s != 0) *e++ = *s++;
        s = name;
        while (*s != 0) *e++ = *s++;
        *e = 0;
        size = xglCdGetFileSize(path);
    }
    readreq(path, buffer);
    return size;
}

extern int func_A1CF18(char *, int);
extern char *MapGetName(int map_id);
typedef struct RESMapView {
    float (*mtx)[4];
    void *p4;
    void *p8;
    void *pC;
    void *p10;
    unsigned char pad[12];
} RESMapView;

static int RES_loadFileSubMap(void *arg0, int arg1)
{
  void *m;
  float (*temp_4)[4];
  unsigned char buf[256];
  unsigned int var_16;
  int var_4;
  unsigned short af;
  var_16 = (unsigned int)arg0;
  m = arg0;
  temp_4 = (float (*)[4]) (32 + ((char *) arg0));
  ((RESMapView *) m)->mtx = temp_4;
  xglMatrixUnit(temp_4);
  var_16 = (int) (((RESMapView *) m)->mtx + 4);
  temp_4 = (float (*)[4]) var_16;
  arg0 = MapGetName(arg1);

  af = arcfileaddr != 0;
  ((RESMapView *) m)->p4 = (void *) temp_4;
  if (af)
  {
    var_4 = next_arc_size((void *) var_16);
  }
  else
  {
    var_4 = RES_loadFileSubMapSub(arg0, ".lex", (void *) var_16);
  }
  var_16 = ((var_16 + var_4) + 0xF) & (~0xF);
  af = arcfileaddr != 0;
  ((RESMapView *) m)->p8 = (void *) var_16;
  if (af)
  {
    var_4 = next_arc_size((void *) var_16);
  }
  else
  {
    var_4 = RES_loadFileSubMapSub(arg0, ".xtx", (void *) var_16);
  }
  if (var_4 <= 0)
  {
    (*((RESMapView *) m)).p8 = 0;
  }
  else
  {
    var_16 = ((var_16 + var_4) + 0xF) & (~0xF);
  }
  af = arcfileaddr != 0;
  ((RESMapView *) m)->pC = (void *) var_16;
  if (af)
  {
    var_4 = next_arc_size((void *) var_16);
  }
  else
  {
    var_4 = RES_loadFileSubMapSub(arg0, ".id", (void *) var_16);
  }
  if (var_4 <= 0)
  {
    ((RESMapView *) m)->pC = 0;
  }
  else
  {
    var_16 = ((var_16 + var_4) + 0xF) & (~0xF);
  }
  ((RESMapView *) m)->p10 = (void *) var_16;
  if (func_A1CF18((char *) buf, arg1) != 0)
  {
    if (arcfileaddr != 0)
    {
      var_4 = next_arc_size(((RESMapView *) m)->p10);
    }
    else
    {
      var_4 = xglCdReadFile((char *) buf, ((RESMapView *) m)->p10, 0, 1);
    }
  }
  else
  {
    var_4 = -1;
  }
  if (var_4 <= 0)
  {
    ((RESMapView *) m)->p10 = 0;
  }
  else
  {
    var_16 = ((var_16 + var_4) + 0xF) & (~0xF);
  }
  return ((int) var_16) - ((int) m);
}

extern void GameResourceReset(int reset_mode);
char *RES_GetMotBaseName(u16 id)
{
  CfMtnEntry *p;
  u16 *q;
  char *ret;
  int cur;
  for (p = cfMtnList; p->name != 0; p++)
  {
    for (q = p->ids; (*q) != 0; q++)
    {
      cur = *q;
      ;
      if (cur == id)
      {
        return p->name;
      }
    }

  }

 do { } while (0);
  return cfMtnList->name;
}

static int convert_face(int id);
static inline char *scopy(char *d, char *s)
{
    while ((*d = *s) != 0) {
        d++;
        s++;
    }
    return d;
}

static int RES_GetMdlFileNameSub(char *name, int key)
{
    static char root_4[6] __asm__("root.4") = "data\\";
    static char dummy_5[6] __asm__("dummy.5") = "dummy";
    char *suffix = 0;
    int id = key & 0xFFFF;
    int n;
    int k;
    ResourceKeyEntry *top;
    ResourceKeyEntry *sub;
    char **list;
    char *out;
    char *s;
    char *t;
    char c;
    unsigned char u;

    if ((key & 0xFFFF0000) == 0x10000) {
        suffix = "_face";
        id = convert_face(id);
    }
    top = (ResourceKeyEntry *)model + ((id & 0x7000) >> 12);
    k = (id & 0xF00) >> 8;
    n = id & 0xFF;
    if (n == 0) {
        out = name;
        s = root_4;
        while ((*out = *s) != 0) {
            out++;
            s++;
        }
        s = (char *)top->unmodeled_08;
        while ((*out = *s) != 0) {
            out++;
            s++;
        }
        s = dummy_5;
        while ((*out = *s) != 0) {
            out++;
            s++;
        }
        if (suffix != 0) {
            out = scopy(out, suffix);
        }
        return id;
    }
    sub = (ResourceKeyEntry *)top->aliases + k;
    list = (char **)sub->aliases;
    while (list != 0) {
        if (--n == 0) {
            break;
        }
        list++;
    }
    if (list != 0) {
        out = name;
        t = root_4;
        while ((*out = *t) != 0) {
            out++;
            t++;
        }
        t = (char *)top->unmodeled_08;
        while ((*out = *t) != 0) {
            out++;
            t++;
        }
        t = (char *)sub->unmodeled_08;
        while ((*out = *t) != 0) {
            out++;
            t++;
        }
        t = *list;
        if (suffix != 0) {
            if (*t == '+') {
                t++;
            }
            while ((c = *t++) != 0 && c != ',') {
                *out++ = c;
            }
            t = suffix;
            while ((*out = *t) != 0) {
                out++;
                t++;
            }
        } else {
            u = *t;
            if (*t == '+') {
                c = ',';
                t++;
                while ((char)(u = *t++) != c) {
                }
                u = *t;
            }
            *out = u;
            if (u != 0) {
                do {
                    out++;
                    t++;
                } while ((*out = *t) != 0);
            }
        }
        return id;
    }
    *name = 0;
    return 0;
}

extern u8 ModelPath;

void RES_GetMdlFileName(char *name, int charaId)
{
    char buf[256];
    char *src;
    char *dst;
    int done;

    if (ModelPath == 0) {
        RES_GetMdlFileNameSub(name, charaId);
        return;
    }
    RES_GetMdlFileNameSub(buf, charaId);
    src = buf;
    dst = name;
    done = 0;
    for (; (*dst = *src) != 0; dst++, src++) {
        if (*src == '\\') {
            if (done == 0) {
                done = 1;
                dst[5] = '\\';
                dst[1] = 't';
                dst[2] = 'e';
                dst[3] = 's';
                dst[4] = 't';
                dst += 5;
            }
        }
    }
}

typedef struct {
    void *first_file;
    void *second_file;
    void *third_file;
    u8 unmodeled_0c[4];
} ResourceFileSet;

static int RES_loadFileSub(int callback, int resource_id, int address_word)
{
    static char base_6[] __asm__("base.6") = "data\\motion\\";
    static char base_7[] __asm__("base.7") = "data\\movie\\small\\mvs0000.ipu";
    static char base_8[] __asm__("base.8") = "data\\texture\\fm0000.xtx";
    void *address = (void *)address_word;
    char single_path[256];
    char model_path[256];
    char motion_path[256];
    char named_path[256];
    char effect_path[256];
    char movie_path[256];
    char texture_path[256];
    char *path;
    char *end;
    const char *source;
    int size = 0;
    int type = callback & 0xffff;
    int quotient;

    switch (type) {
    case 7: {
        char *single_end;

        RES_GetMdlFileName(single_path, resource_id);
        single_end = single_path;
        while (*single_end != 0)
            single_end++;
        if (arcfileaddr != 0) {
            size = next_arc_size(address);
            break;
        }
        __builtin_memcpy(single_end, ".jnt", 5);
        size = xglCdGetFileSize(single_path);
        if (size > 0)
            readreq(single_path, (void *)address);
        break;
    }
    case 0:
    case 4: {
        char *model_end;
        u8 *next;
        ResourceFileSet *files;

        next = (u8 *)address + 16;
        RES_GetMdlFileName(model_path, resource_id);
        model_end = model_path;
        while (*model_end != 0)
            model_end++;
        files = (ResourceFileSet *)address;
        files->first_file = 0;
        files->second_file = 0;
        files->third_file = 0;
        if (arcfileaddr != 0) {
            size = next_arc_size(next);
            if (size <= 0)
                break;
        } else {
            __builtin_memcpy(model_end, ".lex", 5);
            size = xglCdGetFileSize(model_path);
            if (size <= 0)
                break;
            readreq(model_path, next);
        }
        files->first_file = next;
        next += (size + 15) & ~15;
        if ((resource_id & 0xffff0000) == 0) {
            if (arcfileaddr != 0) {
                size = next_arc_size(next);
            } else {
                __builtin_memcpy(model_end, ".xtx", 5);
                size = xglCdGetFileSize(model_path);
                if (size > 0)
                    readreq(model_path, next);
            }
            if (size > 0) {
                files->second_file = next;
                next += (size + 15) & ~15;
            }
        }
        if (type == 0) {
            if (arcfileaddr != 0) {
                size = next_arc_size(next);
            } else {
                __builtin_memcpy(model_end, ".jnt", 5);
                size = xglCdGetFileSize(model_path);
                if (size > 0)
                    readreq(model_path, next);
            }
            if (size > 0) {
                files->third_file = next;
                next += (size + 15) & ~15;
            }
        }
        size = (u32)next - (u32)address;
        break;
    }
    case 1:
        size = RES_loadFileSubMap(address, resource_id);
        break;
    case 2:
        if (arcfileaddr != 0) {
            size = next_arc_size(address);
            break;
        }
        sprintf(motion_path, "data\\motion\\%s.fpk", ((char *(*)(int))RES_GetMotBaseName)(resource_id));
        size = xglCdGetFileSize(motion_path);
        if (size > 0) {
            size = xglCdGetFileSize(motion_path);
            readreq(motion_path, (void *)address);
        }
        break;
    case 3: {
        int append_suffix;
        char *named_end;
        const char *named_source;
        const char *resource_name = (const char *)resource_id;

        if (arcfileaddr != 0) {
            size = next_arc_size(address);
            break;
        }
        named_end = named_path;
        named_source = resource_name + 1;
        if (resource_name[0] != '\\') {
            named_source = base_6;
            while ((*named_end = *named_source) != 0) {
                named_end++;
                named_source++;
            }
            named_source = resource_name;
        }
        *named_end = *named_source;
        append_suffix = 1;
        while (*named_end != 0) {
            if (*named_source == '.')
                append_suffix = 0;
            named_source++;
            named_end++;
            *named_end = *named_source;
        }
        if (append_suffix) {
            named_end[0] = '.';
            named_end[1] = 'f';
            named_end[2] = 'p';
            named_end[3] = 'k';
            named_end[4] = 0;
        }
        size = xglCdGetFileSize(named_path);
        if (size > 0) {
            size = xglCdGetFileSize(named_path);
            readreq(named_path, (void *)address);
        }
        break;
    }
    case 10:
        if (arcfileaddr != 0) {
            size = next_arc_size(address);
            break;
        }
        path = effect_path;
        end = path;
        source = base_7;
        while ((*end = *source) != 0) {
            end++;
            source++;
        }
        goto format_numbered_path;
    case 11:
        if (arcfileaddr != 0) {
            size = next_arc_size(address);
            break;
        }
        path = movie_path;
        end = path;
        source = base_8;
        while ((*end = *source) != 0) {
            end++;
            source++;
        }

format_numbered_path:
        quotient = resource_id;
        end[-5] = (u8)end[-5] + quotient % 10;
        quotient /= 10;
        end[-6] = (u8)end[-6] + quotient % 10;
        quotient /= 10;
        end[-7] = (u8)end[-7] + quotient % 10;
        quotient /= 10;
        end[-8] = (u8)end[-8] + quotient % 10;
        size = xglCdGetFileSize(path);
        if (size > 0)
            readreq(path, (void *)address);
        break;
    case 5: {
        char *texture_end;
        const char *texture_source;
        static char base_9[6] __asm__("base.9") = "data\\";

        texture_end = texture_path;
        texture_source = base_9;
        while ((*texture_end = *texture_source) != 0) {
            texture_end++;
            texture_source++;
        }
        texture_source = (const char *)resource_id;
        while ((*texture_end = *texture_source) != 0) {
            texture_end++;
            texture_source++;
        }
        *texture_end = 0;
        size = xglCdGetFileSize(texture_path);
        if (size > 0) {
            int offset = callback >> 16;

            size += offset;
            readreq(texture_path, (u8 *)address + offset);
        }
        break;
    }
    default:
        break;
    }

    return size;
}

extern int RES_loadFileSub(int callback, int resource_id, int address);

int RES_loadFile(int command, int callback, int resource_id, int flags)
{
    int sp4;
    int sp0;
    int var_20;
    int t;
    int size;
    int used;

    sp0 = callback;
    sp4 = resource_id;
    var_20 = 0;
    if (command == -1) {
        command = GameResourceSearch(callback, resource_id);
        if (command < 0) {
            if (sp0 == 4) {
                command = GameResourceSearch(0, sp4);
            } else if (sp0 == 0) {
                command = GameResourceSearch(4, sp4);
                if (command >= 0) {
                    u32 addr = GameResource[command].address;
                    if (*(int *)(addr + 8) == 0) {
                        t = resource_get_free();
                        *(void **)(addr + 8) = (void *)GameResource[t].address;
                        RES_loadFile(t, 7, sp4, flags);
                    }
                }
            }
        }
        if (command >= 0) {
            var_20 = GameResource[command].address;
        } else {
            command = resource_get_free();
            if (command != -1) {
                var_20 = RES_loadFile(command, sp0, sp4, flags);
            }
        }
        return var_20;
    }
    if (command < 0x80) {
        var_20 = GameResource[command].address;
        if (flags == 0) {
            flags = (int)RES_loadFileSub;
        }
        used = ((int (*)(int, int, int))flags)(callback, resource_id, var_20);
        if (used < 0) {
            return 0;
        }
        size = GameResource[command].size;
        used = (used + 0x3F) & ~0x3F;
        used = (size < used) ? size : used;
        if (command < 0x7F) {
            if (GameResource[command + 1].address == 0) {
                GameResource[command + 1].size = size - used;
                GameResource[command + 1].address = var_20 + used;
                GameResource[command].size = used;
            }
        }
        resource_typeid_translate(command, &sp0, &sp4);
        GameResource[command].state = sp0;
        GameResource[command].handle = sp4;
    }
    return var_20;
}

int GameResourceGetIndex(int id)
{
    int i;
    int *p = (int *)&GameResource[0].address;
    for (i = 0; i < 128; i++) {
        int v = *p;
        p += 4;
        if (v == id) return i;
        if (v == 0) break;
    }
    return -1;
}

extern void xglFontDebugPrintf(int x, int y, const char *format, ...);

void GameResourceDump(int dump)
{
    GameResourceEntry *p;
    GameResourceEntry *q;
    int i;
    int n;
    int y;

    if (dump != 0) {
        i = 0x7F;
        if (GameResource[i].address == 0) {
            do {
                i--;
                if (GameResource[i].address != 0) break;
            } while (i != 0);
        }
        i -= 0xE;

        if (i < 0) i = 0;

        for (n = 0; n < 16; n++) {
            xglFontDebugPrintf(0x40, 0x40 + n * 8, "%2d:%8x,%9d,%2d,%4x", i, GameResource[i].address, GameResource[i].size, GameResource[i].state, GameResource[i].handle);
            i++;

        }
    }
}

extern void xglSoundSendEffect(void *swd, void *sed, int bank);

void GameResourceReset(int reset_mode)
{
    int i;
    int total;

    total = 0;
    for (i = reset_mode; i < 0x80; i++) {
        total += GameResource[i].size;
    }
    GameResource[reset_mode].size = total;
    GameResource[reset_mode].state = -1;
    GameResource[reset_mode].handle = 0;
    for (i = reset_mode + 1; i < 0x80; i++) {
        GameResource[i].address = 0;
        GameResource[i].size = 0;
        GameResource[i].handle = 0;
        GameResource[i].state = -1;
    }
    for (i = 0; i < 8; i++) {
        EnemySeBank[i].id = 0;
        EnemySeBank[i].type = 0;
        EnemySeBank[i].name[0] = 0;
        xglSoundSendEffect(0, 0, i + 4);
    }
    GameResourceDump(0);
    arcfilepreload = 0;
}

extern void (*GameResourceReadFileCallback)(void);

void GameResourceInit(u32 addr, int size)
{
    int i;
    for (i = 0; i < 128; i++) {
        GameResource[i].address = 0;
        GameResource[i].size = 0;
        GameResource[i].handle = 0;
        GameResource[i].state = -1;
    }
    GameResource[0].address = addr;
    GameResource[0].size = size;
    GameResourceReset(0);
    *(int *)0x01000000 = 0;
    *(int *)0x01070800 = 0;
    GameResourceReadFileCallback = default_callback;
    *(int *)0x010B1000 = 0;
    *(int *)0x010B4000 = 0;
    *(int *)0x010E6000 = 0;
    *(int *)0x010E7000 = 0;
}

GameResourceEntry GameResource[128] = {{0}};
u8 ModelPath = 0;
u8 ScenePath = 0;
void (*GameResourceReadFileCallback)(void) = 0;

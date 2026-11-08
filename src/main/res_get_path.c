#include "common.h"
#include "shared.h"

static char *command_enenpcse(int command, char *cursor, int mode);
extern void GameDiskChange(int disk_number);
extern void Intermission(int save_number);
extern int xglCdGetFileSize(const char *name);
extern int arcfilepreload;
extern char scene_txt_buffer[];
extern void SCRIPT_fade(int time);
static char *command_mpeg2_core(int command, char *cursor, int mode);
static int next_arc_size(void *address);
extern void Enemy_LoadPreset(void *address, const char *name);
extern u32 *arcfileaddr;
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

extern EnemySeEntry EnemySeBank[8];
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

extern const char D_004BF7A0[];
extern const char D_004BF7B0[];
extern const char D_004BF7C8[];
extern const char D_004BF7E0[];
extern const char D_004BF7F0[];
extern const char D_004BF800[];
extern const char D_004BF818[];
extern const char D_004BF828[];
extern const char D_004BF838[];
extern const char D_004BF848[];
extern const char D_004BF858[];
extern const char D_004BF868[];
extern const char D_004BF878[];
extern const char D_004BF888[];
extern const char D_004BF898[];
extern const char D_004BF8B0[];
extern const char D_004BF8C0[];
extern const char D_004BF8D0[];
extern const char D_004BF8E0[];
extern const char D_004BF8F0[];
extern const char D_004BF900[];
extern const char D_004BF918[];
extern const char D_004BF928[];
extern const char D_004BF938[];
extern const char D_004BF950[];
extern const char D_004BF968[];
extern const char D_004BF980[];
extern const char D_004BF998[];
extern const char D_004BF9B0[];
extern const char D_004BF9C8[];
extern const char D_004BF9E0[];
extern const char D_004BF9F0[];
extern const char D_004BFA00[];
extern const char D_004BFA10[];
extern const char D_004BFA20[];
extern const char D_004BFA30[];
extern const char D_004BFA40[];
extern const char D_004BFA58[];
extern const char D_004BFA68[];
extern const char D_004BFA78[];
extern const char D_004BFA88[];
extern const char D_004BFA98[];
extern const char D_004BFAA8[];
extern const char D_004BFAB8[];
extern const char D_004BFAC8[];
extern const char D_004BFAD8[];
extern const char D_004BFAE8[];
extern const char D_004BFAF8[];
extern const char D_004BFB08[];
extern const char D_004BFB18[];
extern const char D_004BFB28[];
extern const char D_004BFB38[];
extern const char D_004BFB48[];
extern const char D_004BFB58[];
extern const char D_004BFB68[];
extern const char D_004BFB78[];
extern const char D_004BFB88[];
extern const char D_004BFB98[];
extern const char D_004BFBA8[];
extern const char D_004BFBB8[];
extern const char D_004BFBC8[];
extern const char D_004BFBD8[];
extern const char D_004BFBE8[];
extern const char D_004BFBF8[];
extern const char D_004BFC08[];
extern const char D_004BFC18[];
extern const char D_004BFC28[];
extern const char D_004BFC38[];
extern const char D_004BFC48[];
extern const char D_004BFC58[];
extern const char D_004BFC68[];
extern const char D_004BFC78[];
extern const char D_004BFC88[];
extern const char D_004BFC98[];
extern const char D_004BFCA8[];
extern const char D_004BFCB8[];
extern const char D_004BFCC8[];
extern const char D_004BFCD8[];
extern const char D_004BFCE8[];
extern const char D_004BFCF8[];
extern const char D_004BFD08[];
extern const char D_004BFD18[];
extern const char D_004BFD28[];
extern const char D_004BFD38[];
extern const char D_004BFD48[];
extern const char D_004BFD58[];
extern const char D_004BFD68[];
extern const char D_004BFD78[];
extern const char D_004BFD88[];
extern const char D_004BFD98[];
extern const char D_004BFDA8[];
extern const char D_004BFDB8[];
extern const char D_004BFDC8[];
extern const char D_004BFDD8[];
extern const char D_004BFDE8[];
extern const char D_004BFDF8[];
extern const char D_004BFE08[];
extern const char D_004BFE18[];
extern const char D_004BFE28[];
extern const char D_004BFE38[];
extern const char D_004BFE48[];
extern const char D_004BFE58[];
extern const char D_004BFE68[];
extern const char D_004BFE78[];
extern const char D_004BFE88[];
extern const char D_004BFE98[];
extern const char D_004BFEA8[];
extern const char D_004BFEB8[];
extern const char D_004BFEC8[];
extern const char D_004D8AA8[];
extern const char D_004D8AB0[];
extern const char D_004D8AB8[];
extern const char D_004D8AC0[];
extern const char D_004D8AC8[];
extern const char D_004D8AD0[];
extern const char D_004D8AD8[];
extern const char D_004D8AE0[];
extern const char D_004D8AE8[];
extern const char D_004D8AF0[];
extern const char D_004D8AF8[];
extern const char D_004D8B00[];
extern const char D_004D8B08[];
extern const char D_004D8B10[];
extern const char D_004D8B18[];
extern const char D_004D8B20[];
extern const char D_004D8B28[];
extern const char D_004D8B30[];
extern const char D_004D8B38[];
extern const char D_004D8B40[];
extern const char D_004D8B48[];
extern const char D_004D8B50[];
extern const char D_004D8B58[];
extern const char D_004D8B60[];
extern const char D_004D8B68[];
extern const char D_004D8B70[];
extern const char D_004D8B78[];
extern const char D_004D8B80[];
extern const char D_004D8B88[];
extern const char D_004D8B90[];
extern const char D_004D8B98[];
extern const char D_004D8BA0[];
extern const char D_004D8BA8[];
extern const char D_004D8BB0[];
extern const char D_004D8BB8[];
extern const char D_004D8BC0[];
extern const char D_004D8BC8[];
extern const char D_004D8BD0[];
extern const char D_004D8BD8[];
extern const char D_004D8BE0[];
extern const char D_004D8BE8[];
extern const char D_004D8BF0[];
extern const char D_004D8BF8[];
extern const char D_004D8C00[];
extern const char D_004D8C08[];
extern const char D_004D8C10[];
extern const char D_004D8C18[];
extern const char D_004D8C20[];
extern const char D_004D8C28[];
extern const char D_004D8C30[];
extern const char D_004D8C38[];
extern const char D_004D8C40[];
extern const char D_004D8C48[];
extern const char D_004D8C50[];
extern const char D_004D8C58[];
extern const char D_004D8C60[];
extern const char D_004D8C68[];
extern const char D_004D8C70[];
extern const char D_004D8C78[];
extern const char D_004D8C80[];
extern const char D_004D8C88[];
extern const char D_004D8C90[];
extern const char D_004D8C98[];
extern const char D_004D8CA0[];
extern const char D_004D8CA8[];
extern const char D_004D8CB0[];
extern const char D_004D8CB8[];
extern const char D_004D8CC0[];
extern const char D_004D8CC8[];
extern const char D_004D8CD0[];
extern const char D_004D8CD8[];
extern const char D_004D8CE0[];
extern const char D_004D8CE8[];
extern const char D_004D8CF0[];
extern const char D_004D8CF8[];
extern const char D_004D8D00[];
extern const char D_004D8D08[];
extern const char D_004D8D10[];
extern const char D_004D8D18[];
extern const char D_004D8D20[];
extern const char D_004D8D28[];
extern const char D_004D8D30[];
extern const char D_004D8D38[];
extern const char D_004D8D40[];
extern const char D_004D8D48[];
extern const char D_004D8D50[];
extern const char D_004D8D58[];
extern const char D_004D8D60[];
extern const char D_004D8D68[];
extern const char D_004D8D70[];
extern const char D_004D8D78[];
extern const char D_004D8D80[];
extern const char D_004D8D88[];
extern const char D_004D8D90[];
extern const char D_004D8D98[];
extern const char D_004D8DA0[];
extern const char D_004D8DA8[];
extern const char D_004D8DB0[];
extern const char D_004D8DB8[];
extern const char D_004D8DC0[];
extern const char D_004D8DC8[];
extern const char D_004D8DD0[];
extern const char D_004D8DD8[];
extern const char D_004D8DE0[];
extern const char D_004D8DE8[];
extern const char D_004D8DF0[];
extern const char D_004D8DF8[];
extern const char D_004D8E00[];
extern const char D_004D8E08[];
extern const char D_004D8E10[];
extern const char D_004D8E18[];
extern const char D_004D8E20[];
extern const char D_004D8E28[];
extern const char D_004D8E30[];
extern const char D_004D8E38[];
extern const char D_004D8E40[];
extern const char D_004D8E48[];
extern const char D_004D8E50[];
extern const char D_004D8E58[];
extern const char D_004D8E60[];
extern const char D_004D8E68[];
extern const char D_004D8E70[];
extern const char D_004D8E78[];
extern const char D_004D8E80[];
extern const char D_004D8E88[];
extern const char D_004D8E90[];
extern const char D_004D8E98[];
extern const char D_004D8EA0[];
extern const char D_004D8EA8[];
extern const char D_004D8EB0[];
extern const char D_004D8EB8[];
extern const char D_004D8EC0[];
extern const char D_004D8EC8[];
extern const char D_004D8ED0[];
extern const char D_004D8ED8[];
extern const char D_004D8EE0[];
extern const char D_004D8EE8[];
extern const char D_004D8EF0[];
extern const char D_004D8EF8[];
extern const char D_004D8F00[];
extern const char D_004D8F08[];
extern const char D_004D8F10[];
extern const char D_004D8F18[];
extern const char D_004D8F20[];
extern const char D_004D8F28[];
extern const char D_004D8F30[];
extern const char D_004D8F38[];
extern const char D_004D8F40[];
extern const char D_004D8F48[];
extern const char D_004D8F50[];
extern const char D_004D8F58[];
extern const char D_004D8F60[];
extern const char D_004D8F68[];
extern const char D_004D8F70[];
extern const char D_004D8F78[];
extern const char D_004D8F80[];
extern const char D_004D8F88[];
extern const char D_004D8F90[];
extern const char D_004D8F98[];
extern const char D_004D8FA0[];
extern const char D_004D8FA8[];
extern const char D_004D8FB0[];
extern const char D_004D8FB8[];
extern const char D_004D8FC0[];
extern const char D_004D8FC8[];
extern const char D_004D8FD0[];
extern const char D_004D8FD8[];
extern const char D_004D8FE0[];
extern const char D_004D8FE8[];
extern const char D_004D8FF0[];
extern const char D_004D8FF8[];
extern const char D_004D9000[];
extern const char D_004D9008[];
extern const char D_004D9010[];
extern const char D_004D9018[];
extern const char D_004D9020[];
extern const char D_004D9028[];
extern const char D_004D9030[];
extern const char D_004D9038[];
extern const char D_004D9040[];
extern const char D_004D9048[];
extern const char D_004D9050[];
extern const char D_004D9058[];
extern const char D_004D9060[];
extern const char D_004D9068[];
extern const char D_004D9070[];
extern const char D_004D9078[];
extern const char D_004D9080[];
extern const char D_004D9088[];
extern const char D_004D9090[];
extern const char D_004D9098[];
extern const char D_004D90A0[];
extern const char D_004D90A8[];
extern const char D_004D90B0[];
extern const char D_004D90B8[];
extern const char D_004D90C0[];
extern const char D_004D90C8[];
extern const char D_004D90D0[];
extern const char D_004D90D8[];
extern const char D_004D90E0[];
extern const char D_004D90E8[];
extern const char D_004D90F0[];
extern const char D_004D90F8[];
extern const char D_004D9100[];
extern const char D_004D9108[];
extern const char D_004D9110[];
extern const char D_004D9118[];
extern const char D_004D9120[];
extern const char D_004D9128[];
extern const char D_004D9130[];
extern const char D_004D9138[];
extern const char D_004D9140[];
extern const char D_004D9148[];
extern const char D_004D9150[];
extern const char D_004D9158[];
extern const char D_004D9160[];
extern const char D_004D9168[];
extern const char D_004D9170[];
extern const char D_004D9178[];
extern const char D_004D9180[];
extern const char D_004D9188[];
extern const char D_004D9190[];
extern const char D_004D9198[];
extern const char D_004D91A0[];
extern const char D_004D91A8[];
extern const char D_004D91B0[];
extern const char D_004D91B8[];
extern const char D_004D91C0[];
extern const char D_004D91C8[];
extern const char D_004D91D0[];
extern const char D_004D91D8[];
extern const char D_004D91E0[];
extern const char D_004D91E8[];
extern const char D_004D91F0[];
extern const char D_004D91F8[];
extern const char D_004D9200[];
extern const char D_004D9208[];
extern const char D_004D9210[];
extern const char D_004D9218[];
extern const char D_004D9220[];
extern const char D_004D9228[];
extern const char D_004D9230[];
extern const char D_004D9238[];
extern const char D_004D9240[];
extern const char D_004D9248[];
extern const char D_004D9250[];
extern const char D_004D9258[];
extern const char D_004D9260[];
extern const char D_004D9268[];
extern const char D_004D9270[];
extern const char D_004D9278[];
extern const char D_004D9280[];
extern const char D_004D9288[];
extern const char D_004D9290[];
extern const char D_004D9298[];
extern const char D_004D92A0[];
extern const char D_004D92A8[];
extern const char D_004D92B0[];
extern const char D_004D92B8[];
extern const char D_004D92C0[];
extern const char D_004D92C8[];
extern const char D_004D92D0[];
extern const char D_004D92D8[];
extern const char D_004D92E0[];
extern const char D_004D92E8[];
extern const char D_004D92F0[];
extern const char D_004D92F8[];
extern const char D_004D9300[];
extern const char D_004D9308[];
extern const char D_004D9310[];
extern const char D_004D9318[];
extern const char D_004D9320[];
extern const char D_004D9328[];
extern const char D_004D9330[];
extern const char D_004D9338[];
extern const char D_004D9340[];
extern const char D_004D9348[];
extern const char D_004D9350[];
extern const char D_004D9358[];
extern const char D_004D9360[];
extern const char D_004D9368[];
extern const char D_004D9370[];
extern const char D_004D9378[];
extern const char D_004D9380[];
extern const char D_004D9388[];
extern const char D_004D9390[];
extern const char D_004D9398[];
extern const char D_004D93A0[];
extern const char D_004D93A8[];
extern const char D_004D93B0[];
extern const char D_004D93B8[];
extern const char D_004D93C0[];
extern const char D_004D93C8[];
extern const char D_004D93D0[];
extern const char D_004D93D8[];
extern const char D_004D93E0[];
extern const char D_004D93E8[];
extern const char D_004D93F0[];
extern const char D_004D93F8[];
extern const char D_004D9400[];
extern const char D_004D9408[];
extern const char D_004D9410[];
extern const char D_004D9418[];
extern const char D_004D9420[];
extern const char D_004D9428[];
extern const char D_004D9430[];
extern const char D_004D9438[];
extern const char D_004D9440[];
extern const char D_004D9448[];
extern const char D_004D9450[];
extern const char D_004D9458[];
extern const char D_004D9460[];
extern const char D_004D9468[];
extern const char D_004D9470[];
extern const char D_004D9478[];
extern const char D_004D9480[];
extern const char D_004D9488[];
extern const char D_004D9490[];
extern const char D_004D9498[];
extern const char D_004D94A0[];
extern const char D_004D94A8[];
extern const char D_004D94B0[];
extern const char D_004D94B8[];
extern const char D_004D94C0[];
extern const char D_004D94C8[];
extern const char D_004D94D0[];
extern const char D_004D94D8[];
extern const char D_004D94E0[];
extern const char D_004D94E8[];
extern const char D_004D94F0[];
extern const char D_004D94F8[];
extern const char D_004D9500[];
extern const char D_004D9508[];
extern const char D_004D9510[];
extern const char D_004D9518[];
extern const char D_004D9520[];
extern const char D_004D9528[];
extern const char D_004D9530[];
extern const char D_004D9538[];
extern const char D_004D9540[];
extern const char D_004D9548[];
extern const char D_004D9550[];
extern const char D_004D9558[];
extern const char D_004D9560[];
extern const char D_004D9568[];
extern const char D_004D9570[];
extern const char D_004D9578[];
extern const char D_004D9580[];
extern const char D_004D9588[];
extern const char D_004D9590[];
extern const char D_004D9598[];
extern const char D_004D95A0[];
extern const char D_004D95A8[];
extern const char D_004D95B0[];
extern const char D_004D95B8[];
extern const char D_004D95C0[];
extern const char D_004D95C8[];
extern const char D_004D95D0[];
extern const char D_004D95D8[];
extern const char D_004D95E0[];
extern const char D_004D95E8[];
extern const char D_004D95F0[];
extern const char D_004D95F8[];
extern const char D_004D9600[];
extern const char D_004D9608[];
extern const char D_004D9610[];
extern const char D_004D9618[];
extern const char D_004D9620[];
extern const char D_004D9628[];
extern const char D_004D9630[];
extern const char D_004D9638[];
extern const char D_004D9640[];
extern const char D_004D9648[];
extern const char D_004D9650[];
extern const char D_004D9658[];
extern const char D_004D9660[];
extern const char D_004D9668[];
extern const char D_004D9670[];
extern const char D_004D9678[];
extern const char D_004D9680[];
extern const char D_004D9688[];
extern const char D_004D9690[];
extern const char D_004D9698[];
extern const char D_004D96A0[];
extern const char D_004D96A8[];
extern const char D_004D96B0[];
extern const char D_004D96B8[];
extern const char D_004D96C0[];
extern const char D_004D96C8[];
extern const char D_004D96D0[];
extern const char D_004D96D8[];
extern const char D_004D96E0[];
extern const char D_004D96E8[];
extern const char D_004D96F0[];
extern const char D_004D96F8[];
extern const char D_004D9700[];
extern const char D_004D9708[];
extern const char D_004D9710[];
extern const char D_004D9718[];
extern const char D_004D9720[];
extern const char D_004D9728[];
extern const char D_004D9730[];
extern const char D_004D9738[];
extern const char D_004D9740[];
extern const char D_004D9748[];
extern const char D_004D9750[];
extern const char D_004D9758[];
extern const char D_004D9760[];
extern const char D_004D9768[];
extern const char D_004D9770[];
extern const char D_004D9778[];
extern const char D_004D9780[];
extern const char D_004D9788[];
extern const char D_004D9790[];
extern const char D_004D9798[];
extern const char D_004D97A0[];
extern const char D_004D97A8[];
extern const char D_004D97B0[];
extern const char D_004D97B8[];
extern const char D_004D97C0[];
extern const char D_004D97C8[];
extern const char D_004D97D0[];
extern const char D_004D97D8[];
extern const char D_004D97E0[];
extern const char D_004D97E8[];
extern const char D_004D97F0[];
extern const char D_004D97F8[];
extern const char D_004D9800[];
extern const char D_004D9808[];
extern const char D_004D9810[];
extern const char D_004D9818[];
extern const char D_004D9820[];
extern const char D_004D9828[];
extern const char D_004D9830[];
extern const char D_004D9838[];
extern const char D_004D9840[];
extern const char D_004D9848[];
extern const char D_004D9850[];
extern const char D_004D9858[];
extern const char D_004D9860[];
extern const char D_004D9868[];
extern const char D_004D9870[];
extern const char D_004D9878[];
extern const char D_004D9880[];
extern const char D_004D9888[];
extern const char D_004D9890[];
extern const char D_004D9898[];
extern const char D_004D98A0[];
extern const char D_004D98A8[];
extern const char D_004D98B0[];
extern const char D_004D98B8[];
extern const char D_004D98C0[];
extern const char D_004D98C8[];
extern const char D_004D98D0[];
extern const char D_004D98D8[];
extern const char D_004D98E0[];
extern const char D_004D98E8[];
extern const char D_004D98F0[];
extern const char D_004D98F8[];
extern const char D_004D9900[];
extern const char D_004D9908[];
extern const char D_004D9910[];
extern const char D_004D9918[];
extern const char D_004D9920[];
extern const char D_004D9928[];
extern const char D_004D9930[];
extern const char D_004D9938[];
extern const char D_004D9940[];
extern const char D_004D9948[];
extern const char D_004D9950[];
extern const char D_004D9958[];
extern const char D_004D9960[];
extern const char D_004D9968[];
extern const char D_004D9970[];
extern const char D_004D9978[];
extern const char D_004D9980[];
extern const char D_004D9988[];
extern const char D_004D9990[];
extern const char D_004D9998[];
extern const char D_004D99A0[];
extern const char D_004D99A8[];
extern const char D_004D99B0[];
extern const char D_004D99B8[];
extern const char D_004D99C0[];
extern const char D_004D99C8[];
extern const char D_004D99D0[];
extern const char D_004D99D8[];
extern const char D_004D99E0[];
extern const char D_004D99E8[];
extern const char D_004D99F0[];
extern const char D_004D99F8[];
extern const char D_004D9A00[];
extern const char D_004D9A08[];
extern const char D_004D9A10[];
extern const char D_004D9A18[];
extern const char D_004D9A20[];
extern const char D_004D9A28[];
extern const char D_004D9A30[];
extern const char D_004D9A38[];
extern const char D_004D9A40[];
extern const char D_004D9A48[];
extern const char D_004D9A50[];
extern const char D_004D9A58[];
extern const char D_004D9A60[];
extern const char D_004D9A68[];
extern const char D_004D9A70[];
extern const char D_004D9A78[];
extern const char D_004D9A80[];
extern const char D_004D9A88[];
extern const char D_004D9A90[];
extern const char D_004D9A98[];
extern const char D_004D9AA0[];
extern const char D_004D9AA8[];
extern const char D_004D9AB0[];
extern const char D_004D9AB8[];
extern const char D_004D9AC0[];
extern const char D_004D9AC8[];
extern const char D_004D9AD0[];
extern const char D_004D9AD8[];
extern const char D_004D9AE0[];
extern const char D_004D9AE8[];
extern const char D_004D9AF0[];
extern const char D_004D9AF8[];
extern const char D_004D9B00[];
extern const char D_004D9B08[];
extern const char D_004D9B10[];
extern const char D_004D9B18[];
extern const char D_004D9B20[];
extern const char D_004D9B28[];
extern const char D_004D9B30[];
extern const char D_004D9B38[];
extern const char D_004D9B40[];
extern const char D_004D9B48[];
extern const char D_004D9B50[];
extern const char D_004D9B58[];
extern const char D_004D9B60[];
extern const char D_004D9B68[];
extern const char D_004D9B70[];
extern const char D_004D9B78[];
extern const char D_004D9B80[];
extern const char D_004D9B88[];
extern const char D_004D9B90[];
extern const char D_004D9B98[];
extern const char D_004D9BA0[];
extern const char D_004D9BA8[];
extern const char D_004D9BB0[];
extern const char D_004D9BB8[];
extern const char D_004D9BC0[];
extern const char D_004D9BC8[];
extern const char D_004D9BD0[];
extern const char D_004D9BD8[];
extern const char D_004D9BE0[];
extern const char D_004D9BE8[];
extern const char D_004D9BF0[];
extern const char D_004D9BF8[];
extern const char D_004D9C00[];
extern const char D_004D9C08[];
extern const char D_004D9C10[];
extern const char D_004D9C18[];
extern const char D_004D9C20[];
extern const char D_004D9C28[];
extern const char D_004D9C30[];
extern const char D_004D9C38[];
extern const char D_004D9C40[];
extern const char D_004D9C48[];
extern const char D_004D9C50[];
extern const char D_004D9C58[];
extern const char D_004D9C60[];
extern const char D_004D9C68[];
extern const char D_004D9C70[];
extern const char D_004D9C78[];
extern const char D_004D9C80[];
extern const char D_004D9C88[];
extern const char D_004D9C90[];
extern const char D_004D9C98[];
extern const char D_004D9CA0[];
extern const char D_004D9CA8[];
extern const char D_004D9CB0[];
extern const char D_004D9CB8[];
extern const char D_004D9CC0[];
extern const char D_004D9CC8[];
extern const char D_004D9CD0[];
extern const char D_004D9CD8[];
extern const char D_004D9CE0[];
extern const char D_004D9CE8[];
extern const char D_004D9CF0[];
extern const char D_004D9CF8[];
extern const char D_004D9D00[];
extern const char D_004D9D08[];
extern const char D_004D9D10[];
extern const char D_004D9D18[];
extern const char D_004D9D20[];
extern const char D_004D9D28[];
extern const char D_004D9D30[];
extern const char D_004D9D38[];
extern const char D_004D9D40[];
extern const char D_004D9D48[];
extern const char D_004D9D50[];
extern const char D_004D9D58[];
extern const char D_004D9D60[];
extern const char D_004D9D68[];
extern const char D_004D9D70[];
extern const char D_004D9D78[];
extern const char D_004D9D80[];
extern const char D_004D9D88[];
extern const char D_004D9D90[];
extern const char D_004D9D98[];
extern const char D_004D9DA0[];
extern const char D_004D9DA8[];
extern const char D_004D9DB0[];
extern const char D_004D9DB8[];
extern const char D_004D9DC0[];
extern const char D_004D9DC8[];
extern const char D_004D9DD0[];
extern const char D_004D9DD8[];
extern const char D_004D9DE0[];
extern const char D_004D9DE8[];
extern const char D_004D9DF0[];
static const char *CID0[54] = {
    D_004BF8E0, D_004D8BB0, D_004D8BA8, D_004D8BA0,
    D_004D8B98, D_004D8B90, D_004D8B88, D_004BF8D0,
    D_004BF8C0, D_004D8B80, D_004D8B78, D_004D8B70,
    D_004BF8B0, D_004D8B68, D_004D8B60, D_004D8B58,
    D_004D8B50, D_004D8B48, D_004D8B40, D_004D8B38,
    D_004D8B30, D_004D8B28, D_004D8B20, D_004D8B18,
    D_004D8B10, D_004D8B08, D_004D8B00, D_004D8AF8,
    D_004D8AF0, D_004BF898, D_004BF888, D_004D8AE8,
    D_004D8AE0, D_004D8AD8, D_004D8AD0, D_004BF878,
    D_004BF868, D_004BF858, D_004BF848, D_004BF838,
    D_004BF828, D_004BF818, D_004BF800, D_004D8AC8,
    D_004BF7F0, D_004BF7E0, D_004D8AC0, D_004BF7C8,
    D_004BF7B0, D_004D8AB8, D_004D8AB0, D_004D8AA8,
    D_004BF7A0, 0
};
static const char *CID1[89] = {
    D_004D8D80, D_004BFB08, D_004D8D78, D_004BFAF8,
    D_004D8D70, D_004D8D68, D_004D8D60, D_004D8D58,
    D_004D8D50, D_004D8D48, D_004D8D40, D_004D8D38,
    D_004D8D30, D_004D8D28, D_004D8D20, D_004D8D18,
    D_004D8D10, D_004D8D08, D_004D8D00, D_004BFAE8,
    D_004D8CF8, D_004D8CF0, D_004D8CE8, D_004D8CE0,
    D_004D8CD8, D_004D8CD0, D_004D8CC8, D_004D8CC0,
    D_004D8CB8, D_004D8CB0, D_004D8CA8, D_004D8CA0,
    D_004D8C98, D_004D8C90, D_004D8C88, D_004D8C80,
    D_004D8C78, D_004D8C70, D_004D8C68, D_004D8C60,
    D_004D8C58, D_004D8C50, D_004BFAD8, D_004D8C48,
    D_004D8C40, D_004BFAC8, D_004D8C38, D_004D8C30,
    D_004D8C28, D_004BFAB8, D_004BFAA8, D_004D8C20,
    D_004D8C18, D_004D8C10, D_004BFA98, D_004D8C08,
    D_004BFA88, D_004BFA78, D_004BFA68, D_004D8C00,
    D_004D8BF8, D_004BFA58, D_004D8BF0, D_004BFA40,
    D_004D8BE8, D_004BFA30, D_004BFA20, D_004BFA10,
    D_004D8BE0, D_004D8BD8, D_004D8BD0, D_004D8BC8,
    D_004BFA00, D_004BF9F0, D_004BF9E0, D_004BF9C8,
    D_004BF9B0, D_004BF998, D_004BF980, D_004BF968,
    D_004BF950, D_004BF938, D_004BF928, D_004BF918,
    D_004BF900, D_004D8BC0, D_004BF8F0, D_004D8BB8,
    0
};
static const char *CID2[32] = {
    D_004D8E50, D_004D8E48, D_004D8E40, D_004D8E38,
    D_004D8E30, D_004D8E28, D_004D8E20, D_004D8E18,
    D_004D8E10, D_004D8E08, D_004D8E00, D_004D8DF8,
    D_004D8DF0, D_004D8DE8, D_004D8DE0, D_004D8DD8,
    D_004D8DD0, D_004D8DC8, D_004D8DC0, D_004D8DB8,
    D_004D8DB0, D_004D8DA8, D_004D8DA0, D_004D8D98,
    D_004D8D90, D_004BFB58, D_004BFB48, D_004BFB38,
    D_004BFB28, D_004D8D88, D_004BFB18, 0
};
static const char *CID3[29] = {
    D_004D8EE0, D_004D8ED8, D_004D8ED0, D_004D8EC8,
    D_004D8EC0, D_004D8EB8, D_004D8EB0, D_004D8EA8,
    D_004D8EA0, D_004D8E98, D_004D8E90, D_004D8E88,
    D_004D8E80, D_004D8E78, D_004D8E70, D_004D8E68,
    D_004D8E60, D_004D8E60, D_004D8E60, D_004D8E60,
    D_004D8E60, D_004D8E60, D_004D8E60, D_004D8E60,
    D_004D8E60, D_004D8E60, D_004D8E60, D_004D8E58,
    0
};
static const char *CID4[17] = {
    D_004D8F50, D_004D8F48, D_004D8F40, D_004D8F38,
    D_004D8F30, D_004D8F28, D_004D8F20, D_004D8F18,
    D_004D8F10, D_004D8F08, D_004D8F00, D_004D8F00,
    D_004D8F00, D_004D8EF8, D_004D8EF0, D_004D8EE8,
    0
};
static const char *CID5[10] = {
    D_004D8F98, D_004D8F90, D_004D8F88, D_004D8F80,
    D_004D8F78, D_004D8F70, D_004D8F68, D_004D8F60,
    D_004D8F58, 0
};
static const char *CID6[81] = {
    D_004D91B8, D_004D91B0, D_004D91A8, D_004D91A0,
    D_004D9198, D_004D9190, D_004D9188, D_004D9180,
    D_004D9178, D_004D9170, D_004D9168, D_004D9160,
    D_004D9158, D_004D9150, D_004D9148, D_004D9140,
    D_004D9138, D_004D9130, D_004D9128, D_004BFC18,
    D_004BFC08, D_004D9120, D_004BFBF8, D_004BFBE8,
    D_004D9118, D_004D9110, D_004D9108, D_004D9100,
    D_004D90F8, D_004D90F0, D_004D90E8, D_004D90E0,
    D_004D90D8, D_004D90D0, D_004D90C8, D_004D90C0,
    D_004D90B8, D_004D90B0, D_004D90A8, D_004D90A0,
    D_004D9098, D_004D9090, D_004D9088, D_004D9080,
    D_004D9078, D_004D9070, D_004D9068, D_004D9060,
    D_004D9058, D_004BFBD8, D_004BFBC8, D_004D9050,
    D_004BFBB8, D_004BFBA8, D_004D9048, D_004D9040,
    D_004D9038, D_004D9030, D_004D9028, D_004D9020,
    D_004D9018, D_004D9010, D_004D9008, D_004D9000,
    D_004D8FF8, D_004D8FF0, D_004D8FE8, D_004D8FE0,
    D_004D8FD8, D_004BFB98, D_004BFB88, D_004D8FD0,
    D_004D8FC8, D_004BFB78, D_004BFB68, D_004D8FC0,
    D_004D8FB8, D_004D8FB0, D_004D8FA8, D_004D8FA0,
    0
};
static const char *RID0[7] = {
    D_004D91E8, D_004D91E0, D_004D91D8, D_004D91D0,
    D_004D91C8, D_004D91C0, 0
};
static const char *RID1[14] = {
    D_004D9250, D_004D9248, D_004D9240, D_004D9238,
    D_004D9230, D_004D9228, D_004D9220, D_004D9218,
    D_004D9210, D_004D9208, D_004D9200, D_004D91F8,
    D_004D91F0, 0
};
static const char *RID2[11] = {
    D_004D9298, D_004D9290, D_004D9288, D_004D9280,
    D_004D9278, D_004BFC28, D_004D9270, D_004D9268,
    D_004D9260, D_004D9258, 0
};
static const char *WID0[11] = {
    D_004D92E8, D_004D92E0, D_004D92D8, D_004D92D0,
    D_004D92C8, D_004D92C0, D_004D92B8, D_004D92B0,
    D_004D92A8, D_004D92A0, 0
};
static const char *WID1[18] = {
    D_004D9370, D_004D9368, D_004D9360, D_004D9358,
    D_004D9350, D_004D9348, D_004D9340, D_004D9338,
    D_004D9330, D_004D9328, D_004D9320, D_004D9318,
    D_004D9310, D_004D9308, D_004D9300, D_004D92F8,
    D_004D92F0, 0
};
static const char *WID2[6] = {
    D_004D9378, D_004D92B8, D_004D92B0, D_004D92A8,
    D_004D92A0, 0
};
static const char *WID3[9] = {
    D_004D93B8, D_004D93B0, D_004D93A8, D_004D93A0,
    D_004D9398, D_004D9390, D_004D9388, D_004D9380,
    0
};
static const char *WID4[6] = {
    D_004D9378, D_004D93D8, D_004D93D0, D_004D93C8,
    D_004D93C0, 0
};
static const char *WID5[2] __attribute__((section(".data"))) = {
    D_004D93E0, 0
};
static const char *WID6[50] = {
    D_004D9568, D_004D9560, D_004D9558, D_004D9550,
    D_004D9548, D_004D9540, D_004D9538, D_004D9530,
    D_004D9528, D_004D9520, D_004D9518, D_004D9510,
    D_004D9508, D_004D9500, D_004D94F8, D_004D94F0,
    D_004D94E8, D_004D94E0, D_004D94D8, D_004D94D0,
    D_004D94C8, D_004D94C0, D_004D94B8, D_004D94B0,
    D_004D94A8, D_004D94A0, D_004D9498, D_004D9490,
    D_004D9488, D_004D9480, D_004D9478, D_004D9470,
    D_004D9468, D_004D9460, D_004D9458, D_004D9450,
    D_004D9448, D_004D9440, D_004D9438, D_004D9430,
    D_004D9428, D_004D9420, D_004D9418, D_004D9410,
    D_004D9408, D_004D9400, D_004D93F8, D_004D93F0,
    D_004D93E8, 0
};
static const char *WID7[12] = {
    D_004D95B8, D_004D95B0, D_004D95A8, D_004D95A0,
    D_004D9598, D_004D9590, D_004D9588, D_004D9580,
    D_004D9578, D_004D9570, D_004BFC38, 0
};
static const char *WID8[2] __attribute__((section(".data"))) = {
    D_004D95C0, 0
};
static const char *EID0[56] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, D_004D9710, D_004D9708,
    D_004D9700, D_004D96F8, D_004D96F0, D_004D96E8,
    D_004D96E0, D_004D96D8, D_004D96D0, D_004D96C8,
    D_004D96C0, D_004D96B8, D_004D96B0, D_004D96A8,
    D_004D96A0, D_004D9698, D_004D9690, D_004D9688,
    D_004D9680, D_004D9678, D_004D9670, D_004D9668,
    D_004D9660, D_004D9658, D_004D9650, D_004D9648,
    D_004D9640, D_004D9638, D_004D9630, D_004D9628,
    D_004D9620, D_004D9618, D_004D9610, D_004D9608,
    D_004D9600, D_004D95F8, D_004D95F0, D_004D95E8,
    D_004D95E0, D_004D95D8, D_004D95D0, D_004D95D0,
    D_004D95D0, D_004D95D0, D_004D95C8, 0
};
static const char *EID1[15] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, D_004D9710, D_004D9708,
    D_004D9700, D_004D96F8, 0
};
static const char *EID2[7] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, 0
};
static const char *EID3[8] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, 0
};
static const char *EID4[8] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, 0
};
static const char *EID5[7] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, 0
};
static const char *EID6[2] __attribute__((section(".data"))) = {
    D_004D9760, 0
};
static const char *EID7[5] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    0
};
static const char *EID8[4] = {
    D_004D9760, D_004D9758, D_004D9750, 0
};
static const char *EIDf[34] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, D_004D9710, D_004D9708,
    D_004D9700, D_004D96F8, D_004D96F0, D_004D96E8,
    D_004D96E0, D_004D96D8, D_004D96D0, D_004D96C8,
    D_004D96C0, D_004D96B8, D_004D96B0, D_004D96A8,
    D_004D96A0, D_004D97A0, D_004D9798, D_004D9790,
    D_004D9788, D_004D9780, D_004D9778, D_004D9770,
    D_004D9768, 0
};
static const char *MID0[182] = {
    D_004D9C38, D_004D9760, D_004D9758, D_004D9750,
    D_004D9748, D_004D9740, D_004D9738, D_004D9730,
    D_004D9728, D_004D9720, D_004D9718, D_004D9710,
    D_004D9708, D_004D9700, D_004D96F8, D_004D96F0,
    D_004D96E8, D_004D96E0, D_004D96D8, D_004D96D0,
    D_004D96C8, D_004D96C0, D_004D96B8, D_004D96B0,
    D_004D96A8, D_004D96A0, D_004D97A0, D_004D9798,
    D_004D9790, D_004D9788, D_004D9780, D_004D9778,
    D_004D9770, D_004D9768, D_004D9C30, D_004D9C28,
    D_004D9C20, D_004D9C18, D_004D9C10, D_004D9C08,
    D_004D9C00, D_004D9BF8, D_004D9BF0, D_004D9BE8,
    D_004D9BE0, D_004D9BD8, D_004D9BD0, D_004D9BC8,
    D_004D9BC0, D_004D9BB8, D_004D9BB0, D_004D9BA8,
    D_004D9BA0, D_004D9B98, D_004D9B90, D_004D9B88,
    D_004D9B80, D_004D9B78, D_004D9B70, D_004D9B68,
    D_004D9B60, D_004D9B58, D_004D9B50, D_004D9B48,
    D_004D9B40, D_004D9B38, D_004D9B30, D_004D9B28,
    D_004D9B20, D_004D9B18, D_004D9B10, D_004D9B08,
    D_004D9B00, D_004D9AF8, D_004D9AF0, D_004D9AE8,
    D_004D9AE0, D_004D9AD8, D_004D9AD0, D_004D9AC8,
    D_004D9AC0, D_004D9AB8, D_004D9AB0, D_004D9AA8,
    D_004D9AA0, D_004D9A98, D_004D9A90, D_004D9A88,
    D_004D9A80, D_004D9A78, D_004D9A70, D_004D9A68,
    D_004D9A60, D_004D9A58, D_004D9A50, D_004D9A48,
    D_004D9A40, D_004D9A38, D_004D9A30, D_004D9A28,
    D_004D9A20, D_004D9A18, D_004D9A10, D_004D9A08,
    D_004D9A00, D_004D99F8, D_004D99F0, D_004D99E8,
    D_004D99E0, D_004D99D8, D_004D99D0, D_004D99C8,
    D_004D99C0, D_004D99B8, D_004D95C8, D_004D99B0,
    D_004D99A8, D_004D99A0, D_004D9998, D_004D9990,
    D_004D9988, D_004D9980, D_004D9978, D_004D9970,
    D_004D9968, D_004D9960, D_004D9958, D_004D9950,
    D_004D9948, D_004D9940, D_004D9938, D_004D9930,
    D_004D9928, D_004D9920, D_004D9918, D_004D9910,
    D_004D9908, D_004D9900, D_004D98F8, D_004D98F0,
    D_004D98E8, D_004D98E0, D_004D98D8, D_004D98D0,
    D_004D98C8, D_004D98C0, D_004D98B8, D_004D98B0,
    D_004D98A8, D_004D98A0, D_004D9898, D_004D9890,
    D_004D9888, D_004D9880, D_004D9878, D_004D9870,
    D_004D9868, D_004D9860, D_004D9858, D_004D9850,
    D_004D9848, D_004D9840, D_004D9838, D_004D9830,
    D_004D9828, D_004D9820, D_004D9818, D_004D9810,
    D_004D9808, D_004D9800, D_004D97F8, D_004D97F0,
    D_004D97E8, D_004D97E0, D_004D97D8, D_004D97D0,
    D_004D97C8, D_004D97C0, D_004D97B8, D_004D97B0,
    D_004D97A8, 0
};
static const char *AID0[128] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, D_004D9710, D_004D9708,
    D_004D9700, D_004D96F8, D_004D96F0, D_004D96E8,
    D_004D96E0, D_004D96D8, D_004D96D0, D_004D96C8,
    D_004D96C0, D_004D96B8, D_004D96B0, D_004D96A8,
    D_004D96A0, D_004D97A0, D_004D9798, D_004D9790,
    D_004D9788, D_004D9780, D_004D9778, D_004D9770,
    D_004D9768, D_004D9C30, D_004D9C28, D_004D9C20,
    D_004D9C18, D_004D9C10, D_004D9C08, D_004D9C00,
    D_004D9BF8, D_004D9BF0, D_004D9BE8, D_004D9BE0,
    D_004D9BD8, D_004D9BD0, D_004D9BC8, D_004D9BC0,
    D_004D9BB8, D_004D9BB0, D_004D9BA8, D_004D9BA0,
    D_004D9B98, D_004D9B90, D_004D9B88, D_004D9B80,
    D_004D9B78, D_004D9B70, D_004D9B68, D_004D9B60,
    D_004D9B58, D_004D9B50, D_004D9B48, D_004D9B40,
    D_004D9B38, D_004D9B30, D_004D9B28, D_004D9B20,
    D_004D9B18, D_004D9B10, D_004D9B08, D_004D9B00,
    D_004D9AF8, D_004D9AF0, D_004D9AE8, D_004D9AE0,
    D_004D9AD8, D_004D9AD0, D_004D9AC8, D_004D9AC0,
    D_004D9AB8, D_004D9AB0, D_004D9AA8, D_004D9AA0,
    D_004D9A98, D_004D9A90, D_004D9A88, D_004D9A80,
    D_004D9A78, D_004D9A70, D_004D9A68, D_004D9A60,
    D_004D9A58, D_004D9A50, D_004D9A48, D_004D9A40,
    D_004D9A38, D_004D9A30, D_004D9A28, D_004D9A20,
    D_004D9A18, D_004D9A10, D_004D9A08, D_004D9A00,
    D_004D99F8, D_004D99F0, D_004D99E8, D_004D99E0,
    D_004D99D8, D_004D99D0, D_004D99C8, D_004D99C0,
    D_004D99B8, D_004D95C8, D_004D99B0, D_004D99A8,
    D_004D99A0, D_004D9998, D_004D9990, D_004D9988,
    D_004D9980, D_004D9978, D_004D9970, D_004D9968,
    D_004D9960, D_004D9958, D_004D9950, 0
};
static const char *MU[131] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, D_004D9710, D_004D9708,
    D_004D9700, D_004D96F8, D_004D96F0, D_004D96E8,
    D_004D96E0, D_004D96D8, D_004D96D0, D_004D96C8,
    D_004D96C0, D_004D96B8, D_004D96B0, D_004D96A8,
    D_004D96A0, D_004D97A0, D_004D9798, D_004D9790,
    D_004D9788, D_004D9780, D_004D9778, D_004D9770,
    D_004D9768, D_004D9C30, D_004D9C28, D_004D9C20,
    D_004D9C18, D_004D9C10, D_004D9C08, D_004D9C00,
    D_004D9BF8, D_004D9BF0, D_004D9BE8, D_004D9BE0,
    D_004D9BD8, D_004D9BD0, D_004D9BC8, D_004D9BC0,
    D_004D9BB8, D_004D9BB0, D_004D9BA8, D_004D9BA0,
    D_004D9B98, D_004D9B90, D_004D9B88, D_004D9B80,
    D_004D9B78, D_004D9B70, D_004D9B68, D_004D9B60,
    D_004D9B58, D_004D9B50, D_004D9B48, D_004D9B40,
    D_004D9B38, D_004D9B30, D_004D9B28, D_004D9B20,
    D_004D9B18, D_004D9B10, D_004D9B08, D_004D9B00,
    D_004D9AF8, D_004D9AF0, D_004D9AE8, D_004D9AE0,
    D_004D9AD8, D_004D9AD0, D_004D9AC8, D_004D9AC0,
    D_004D9AB8, D_004D9AB0, D_004D9AA8, D_004D9AA0,
    D_004D9A98, D_004D9A90, D_004D9A88, D_004D9A80,
    D_004D9A78, D_004D9A70, D_004D9A68, D_004D9A60,
    D_004D9A58, D_004D9A50, D_004D9A48, D_004D9A40,
    D_004D9A38, D_004D9A30, D_004D9A28, D_004D9A20,
    D_004D9A18, D_004D9A10, D_004D9A08, D_004D9A00,
    D_004D99F8, D_004D99F0, D_004D99E8, D_004D99E0,
    D_004D99D8, D_004D99D0, D_004D99C8, D_004D99C0,
    D_004D99B8, D_004D95C8, D_004D99B0, D_004D99A8,
    D_004D99A0, D_004D9998, D_004D9990, D_004D9988,
    D_004D9980, D_004D9978, D_004D9970, D_004D9968,
    D_004D9960, D_004D9958, D_004D9950, D_004D9948,
    D_004D9940, D_004D9938, 0
};
static const char *MUW[21] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, D_004D9710, D_004D9708,
    D_004D9700, D_004D96F8, D_004D96F0, D_004D96E8,
    D_004D96E0, D_004D96D8, D_004D96D0, D_004D96C8,
    0
};
static const char *TRE[11] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, 0
};
static const char *MUX[11] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, 0
};
static const char *MUF[11] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, 0
};
static const char *MUS[11] = {
    D_004D9760, D_004D9758, D_004D9750, D_004D9748,
    D_004D9740, D_004D9738, D_004D9730, D_004D9728,
    D_004D9720, D_004D9718, 0
};
static ResKeyTable CID[8] = {
    {D_004D95D0, CID0, D_004D9C58},
    {D_004D95D0, CID1, D_004D9C50},
    {D_004D95D0, CID2, D_004D9C48},
    {D_004D95D0, CID3, D_004D9C48},
    {D_004D95D0, CID4, D_004D9C48},
    {D_004D95D0, CID5, D_004D9C48},
    {D_004D95D0, CID6, D_004D9C40},
    {0, 0, 0}
};
static ResKeyTable RID[4] = {
    {D_004D9C80, RID0, D_004D9C78},
    {D_004D9C70, RID1, D_004D9C68},
    {D_004D95D0, RID2, D_004D9C60},
    {0, 0, 0}
};
static ResKeyTable WID[10] = {
    {D_004D9D00, WID0, D_004D9CF8},
    {D_004D9CF0, WID1, D_004D9CE8},
    {D_004D9CE0, WID2, D_004D9CD8},
    {D_004D9CD0, WID3, D_004D9CC8},
    {D_004D9CC0, WID4, D_004D9CB8},
    {D_004D9CB0, WID5, D_004D9CA8},
    {D_004D95D0, WID6, D_004D9CA0},
    {D_004D95D0, WID7, D_004D9C98},
    {D_004D9C90, WID8, D_004D9C88},
    {0, 0, 0}
};
static ResKeyTable EID[17] = {
    {D_004D9D58, EID0, D_004BFCD8},
    {D_004D9D50, EID1, D_004BFCC8},
    {D_004D9D48, EID2, D_004BFCB8},
    {D_004D9D40, EID3, D_004BFCA8},
    {D_004D9D38, EID4, D_004BFC98},
    {D_004D9D30, EID5, D_004BFC88},
    {D_004D9D28, EID6, D_004BFC78},
    {D_004D9D20, EID7, D_004BFC68},
    {D_004D9D18, EID8, D_004BFC58},
    {D_004D9D10, EID8, D_004BFCD8},
    {D_004D9D10, EID8, D_004BFCD8},
    {D_004D9D10, EID8, D_004BFCD8},
    {D_004D9D10, EID8, D_004BFCD8},
    {D_004D9D10, EID8, D_004BFCD8},
    {D_004D9D10, EID8, D_004BFCD8},
    {D_004D9D08, EIDf, D_004BFC48},
    {0, 0, 0}
};
static ResKeyTable MID[2] = {
    {D_004D95D0, MID0, D_004BFCE8},
    {0, 0, 0}
};
static ResKeyTable AID[2] = {
    {D_004D95D0, AID0, D_004BFCF8},
    {0, 0, 0}
};
static ResKeyTable MOBJ[10] = {
    {D_004D9D88, MU, D_004BFD58},
    {D_004D9D80, MUW, D_004BFD48},
    {D_004D9D78, TRE, D_004BFD38},
    {D_004D9D78, TRE, D_004BFD38},
    {D_004D9D70, MUX, D_004BFD28},
    {D_004D9D70, MUX, D_004BFD28},
    {D_004D9D68, MUF, D_004BFD18},
    {D_004D9D68, MUF, D_004BFD18},
    {D_004D9D60, MUS, D_004BFD08},
    {0, 0, 0}
};
static ResKeyTable model[9] = {
    {D_004D9DD0, CID, D_004D9DC8},
    {D_004D9DD0, CID, D_004D9DC8},
    {D_004D9DC0, RID, D_004D9CA0},
    {D_004D9DB8, WID, D_004D9DB0},
    {D_004D9DA8, EID, D_004D9C98},
    {D_004D9DA0, MID, D_004D9D98},
    {D_004D9D90, AID, D_004D9D98},
    {D_004D95D0, MOBJ, D_004D9D98},
    {0, 0, 0}
};
CfMtnEntry cfMtnList[31] = {
    {D_004BFEC8, {1, 9, 10, 11, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFEB8, {2, 14, 15, 16, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFEA8, {3, 18, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004D9DF0, {4, 20, 21, 22, 23, 50, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004D9DE8, {5, 24, 25, 26, 47, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFE98, {6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFE88, {7, 28, 29, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFE78, {263, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004D9DE0, {1579, 1582, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004D9DD8, {1549, 1552, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFE68, {1543, 1546, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFE58, {1573, 1576, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFE48, {1289, 542, 523, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFE38, {526, 536, 525, 513, 527, 1288, 537, 519, 0, 0, 0, 0, 0, 0}},
    {D_004BFE28, {1567, 1570, 783, 533, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFE18, {1537, 1540, 780, 530, 529, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFE08, {1591, 1594, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFDF8, {16385, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFDE8, {16410, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFDD8, {16411, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFDC8, {16412, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFDB8, {16413, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFDA8, {16414, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFD98, {16432, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFD88, {16433, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFD78, {16434, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFDE8, {16410, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFDD8, {16411, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFDC8, {16412, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {D_004BFD68, {16416, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
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

extern const char D_004C0098[];
extern const char D_004C0080[];
extern const char D_004C0068[];
extern const char D_004C0058[];
extern const char D_004C0040[];
extern const char D_004C0028[];
extern const char D_004C0010[];
extern const char D_004BFFF8[];
extern const char D_004BFFE0[];
extern const char D_004BFFC8[];
extern const char D_004BFFB0[];
extern const char D_004BFF98[];
extern const char D_004BFF80[];
extern const char D_004BFF68[];
extern const char D_004BFF50[];
extern const char D_004BFF38[];
extern const char D_004BFF20[];
extern const char D_004BFF08[];
extern const char D_004BFEF0[];
extern const char D_004BFED8[];
extern const char D_004D9E20[];
extern const char D_004D9E18[];
extern const char D_004D9E10[];
extern const char D_004D9E08[];
extern const char D_004D9E00[];
extern const char D_004D9DF8[];

const char *RES_getPath(void)
{
    static const char *tbl_0_00366420[20] __asm__("tbl.0") = {
        D_004C0098, D_004C0080, D_004C0068, D_004C0058,
        D_004C0040, D_004C0028, D_004C0010, D_004BFFF8,
        D_004BFFE0, D_004BFFC8, D_004BFFB0, D_004BFF98,
        D_004BFF80, D_004BFF68, D_004BFF50, D_004BFF38,
        D_004BFF20, D_004BFF08, D_004BFEF0, D_004BFED8
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

extern u8 GameResourceWorkReloadLeader;

extern u8 GameResourceWorkReloadTable[];

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

extern u32 archeader[];
extern u8 arcno;
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
extern char foot_name[];

void RES_GetLeaderSeName(char *dst)
{
    static char *tbl_1_00366470[7] __asm__("tbl.1") = {
        (char *)D_004D95D0, (char *)D_004D9E20, (char *)D_004D9E18, (char *)D_004D9E10, (char *)D_004D9E08, (char *)D_004D9E00, (char *)D_004D9DF8
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

extern char env_name[];

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
extern const char D_004D9EA0[];
extern const char D_004D9E98[];
extern const char D_004D9E90[];
extern const char D_004D9E88[];
extern const char D_004D9E80[];
extern const char D_004D9E78[];
extern const char D_004D9E70[];
extern const char D_004D9E68[];
extern const char D_004D9E60[];
extern const char D_004D9E58[];
extern const char D_004D9E50[];
extern const char D_004D9E48[];
extern const char D_004D9E40[];
extern const char D_004D9E38[];
extern const char D_004D9E30[];
extern const char D_004D9E28[];
static GameResourceCommand command[24] = {
    {D_004D9EA0, (char *(*)(int, char *))command_reset, 0},
    {D_004D9E98, (char *(*)(int, char *))command_loadmap, 0},
    {D_004D9E90, (char *(*)(int, char *))command_loadact, 0},
    {"loadface", (char *(*)(int, char *))command_loadface, 0},
    {D_004D9E88, (char *(*)(int, char *))command_loadmot, 0},
    {D_004D9E80, (char *(*)(int, char *))command_loadtex, 0},
    {"loadmovie", (char *(*)(int, char *))command_loadmovie, 0},
    {"loadeffect", (char *(*)(int, char *))command_loadeffect, 0},
    {D_004D9E78, (char *(*)(int, char *))command_loadene, 0},
    {D_004D9E70, (char *(*)(int, char *))command_script, 0},
    {D_004D9E68, (char *(*)(int, char *))command_player, 0},
    {D_004D9E60, (char *(*)(int, char *))command_dummy, 0},
    {D_004D9E58, (char *(*)(int, char *))command_loadse, 0},
    {D_004D9E50, (char *(*)(int, char *))command_loadsmd, 0},
    {D_004D9E48, (char *(*)(int, char *))command_mpeg2, 0},
    {"mpeg2_btl", (char *(*)(int, char *))command_mpeg2battle, 0},
    {"mpeg2_nofade", (char *(*)(int, char *))command_mpeg2nofade, 0},
    {D_004D9E40, (char *(*)(int, char *))command_enese, 0},
    {D_004D9E38, (char *(*)(int, char *))command_npcse, 0},
    {"event_btl", (char *(*)(int, char *))command_event2battle, 0},
    {D_004D9E30, (char *(*)(int, char *))command_disk, 0},
    {D_004D9E28, (char *(*)(int, char *))command_save, 0},
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

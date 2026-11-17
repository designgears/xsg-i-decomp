/*
 * Test rooms: jump straight to a scene, for working on the rebuilt UI.
 *
 * Called once per field-loop frame from Game() (src/main/game.c, PORT_HOST).
 *
 *   debug.jump = cf1234 | ev0123 | <number>
 *       Once the first field scene is running, jump to that scene the way the
 *       developers' debug menu does (Java_xeno_util_Runtime_jump_sub).  cfNNNN
 *       are field scenes (map + its event script), evNNNN story events.
 *   debug.scene_list = 1
 *       At the same point, log every cfNNNN / evNNNN scene file on the disc.
 *
 *   debug.room = <name>
 *       A named room from the table below (same as debug.jump = its scene).
 *   debug.give_all = 1
 *       At the same point, give the whole party, A.G.W.S., money and every
 *       item / weapon / ammo / accessory (implied by the menu rooms).
 *
 * debug.pad entries written "+N:BUTTON" count N vblanks from the moment the
 * jumped-to scene starts (xsg_room_frame), so a room recipe can open a menu
 * without depending on how long the title screen took.
 *
 * Every scene / map change is logged as "[room] vblank V scene cf1234 map
 * <name>", so playing normally also builds the list of rooms worth jumping to.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xsg_rt.h"
#include "xsg_settings.h"

extern const char *RES_getPath(void);
extern int xglCdGetFileSize(const char *name);
extern char *MapGetName(int map_id);
extern void Java_xeno_util_Runtime_jump_sub(int destination, int argument);
extern void PartyFriendOn(int id);
extern void GameStateSave(void);
extern void GameStateSaveCameraLight(void);
extern void GameStateRestoreCameraLight(void);
extern int MenuFileMain(int mode);
extern void MenuShopMain(int shop);
extern void *TMENU_create(int slot);
extern void TMENU_addItem(void *menu, const char *text);
extern int CreateEvtItemGetTask(int category, int id);
extern void Java_xeno_util_Runtime_minigameExec__I(void *thread, int *arguments);
extern void PartyTakeAgwsOn(int id);
extern int dataMoneyBoxInc(int amount);
extern unsigned char itmBox[0x1FE], wpnBox[0x1FE], bltBox[0x1FE], accBox[0x1FE];

/* The vblank the jumped-to scene started on (0: not yet). */
volatile unsigned xsg_room_frame;

/* The developers' cf90xx scenes are battle tests; cf9999 is the game's exit id.
   pad: "+N:..." entries appended to debug.pad (N counts from the scene start).
   The field scene starts walking at about +300; the main menu (TRIANGLE) rows
   are Items, Ether, Tech Attacks, Skills, Characters, A.G.W.S., Battle
   Formation, U.M.N., Game Options. */
#define MENU "+600:TRIANGLE"
#define MENU_ROW(n) MENU ",+680:DOWN*" #n "@12,+800:CIRCLE"
#define MENU_PICK(n) MENU_ROW(n) ",+900:CIRCLE"   /* ... then pick Shion */
/* give: debug.give_all is implied (menu rooms show full lists).
   call: once walking (+ROOM_CALL_AT), open a screen the way the field loop
   does (game.c checkTalkPoint): ROOM_SAVE the save point's file menu,
   ROOM_SHOP + n shop n (1..20, the shop file's records). */
enum { ROOM_SAVE = 1, ROOM_CHOICE = 2, ROOM_ITEM_GET = 3, ROOM_MINIGAME = 0x200, ROOM_SHOP = 0x100 };
#define ROOM_CALL_AT 450
static const struct { const char *name, *scene, *pad, *what; int give, call; } rooms[] = {
    {"battle_tutorial", "cf9001", 0, "Shion + KOS-MOS vs attack drones, tutorial pages and dialog"},
    {"battle_kosmos", "cf9002", 0, "KOS-MOS vs Drone SPX: full command menu, AP, boost"},
    {"battle_win", "cf9002", "+650:SQUARE*900@40,+670:TRIANGLE*900@40,+690:CIRCLE*900@40", "battle_kosmos fought to the end (attacks, then the results)"},
    {"battle_kosmos2", "cf9003", 0, "KOS-MOS vs Drone GX"},
    {"battle_boss", "cf9005", 0, "boss in a corridor, three-member turn list"},
    {"battle", "cf9011", 0, "Shion + KOS-MOS vs U-TIC soldier"},
    {"battle_fx", "cf9021", 0, "battle whose opening is a large effect"},
    {"field", "cf9051", 0, "plays a short scene, then free walking in MC_KOU01"},
    {"pause", "cf9051", "+600:START", "field, then the pause screen", 0},
    {"menu", "cf9051", MENU, "field, then the main menu", 1},
    {"menu_items", "cf9051", MENU ",+800:CIRCLE", "main menu > Items", 1},
    {"menu_ether", "cf9051", MENU_PICK(1), "main menu > Ether > Shion", 1},
    {"menu_ether_tree", "cf9051", MENU_PICK(1) ",+1000:DOWN*2@12,+1100:CIRCLE", "main menu > Ether > Shion > Use E.Pts (the Ether tree)", 1},
    {"menu_tech", "cf9051", MENU_PICK(2), "main menu > Tech Attacks > Shion", 1},
    {"menu_skills", "cf9051", MENU_PICK(3), "main menu > Skills > Shion", 1},
    {"menu_characters", "cf9051", MENU_PICK(4), "main menu > Characters > Shion", 1},
    {"menu_agws", "cf9051", MENU_ROW(5), "main menu > A.G.W.S.", 1},
    {"menu_formation", "cf9051", MENU_ROW(6), "main menu > Battle Formation", 1},
    {"menu_umn", "cf9051", MENU_ROW(7), "main menu, U.M.N. row (still locked: a story flag)", 1},
    {"menu_options", "cf9051", MENU_ROW(8), "main menu > Game Options", 1},
    {"save", "cf9051", 0, "a save point's file screen", 1, ROOM_SAVE},
    {"robot", "cf9051", 0, "the robot battle minigame (ov12)", 1, ROOM_MINIGAME + 12},
    {"xenocard", "cf9051", 0, "the Xenocard minigame (ov10)", 1, ROOM_MINIGAME + 13},
    {"casino", "cf9051", 0, "the casino minigames (ov11)", 1, ROOM_MINIGAME + 14},
    {"item_get", "cf9051", 0, "the item-get popup (a Med Kit)", 0, ROOM_ITEM_GET},
    {"choice", "cf9051", 0, "a choice menu (twsys TMENU), as scripts open them", 0, ROOM_CHOICE},
    {"save_files", "cf9051", "+560:CIRCLE", "save point > memory card slot 1", 1, ROOM_SAVE},
    {"shop", "cf9051", 0, "shop 1", 1, ROOM_SHOP + 1},
    {"shop_buy", "cf9051", "+560:CIRCLE", "shop 1 > Buy", 1, ROOM_SHOP + 1},
    {"shop_sell", "cf9051", "+560:DOWN,+600:CIRCLE", "shop 1 > Sell", 1, ROOM_SHOP + 1},
    {"shop2", "cf9051", 0, "shop 2", 1, ROOM_SHOP + 2},
    {"shop3", "cf9051", 0, "shop 3", 1, ROOM_SHOP + 3},
};
#define NROOMS (sizeof rooms / sizeof rooms[0])

static int room_index(void)
{
    const char *room = xsg_cfg("debug.room");
    if (!room || !*room)
        return -1;
    for (size_t i = 0; i < NROOMS; ++i)
        if (!strcmp(room, rooms[i].name))
            return (int)i;
    return -2;
}

/* sdk.c: the room's pad entries, parsed after debug.pad. */
const char *xsg_room_pad(void)
{
    int i = room_index();
    return i >= 0 ? rooms[i].pad : 0;
}

/* debug.give_all: everything the developers' ShopTest gives (party members,
   A.G.W.S. 17 and 24, money) plus every normal item (x99) and every weapon,
   ammo and accessory (x9).  Counts are the ov01 name tables (data_eth_name_get.c);
   event (key) items are left alone, they drive the story. */
static void give_all(void)
{
    static const struct { unsigned char *box; int n, qty; } boxes[] = {
        {itmBox, 72, 99}, {wpnBox, 114, 9}, {bltBox, 178, 9}, {accBox, 180, 9},
    };
    for (int id = 1; id < 8; ++id)
        PartyFriendOn(id);
    PartyTakeAgwsOn(17);
    PartyTakeAgwsOn(24);
    dataMoneyBoxInc(999999999);
    for (size_t b = 0; b < sizeof boxes / sizeof boxes[0]; ++b) {
        unsigned short *q = (unsigned short *)boxes[b].box;
        for (int i = 0; i < boxes[b].n; ++i)
            if (q[i] < boxes[b].qty)
                q[i] = (unsigned short)boxes[b].qty;
    }
    xsg_logf("[room] give_all: party, A.G.W.S., money, items, weapons, ammo, accessories\n");
}

static void scene_name(char *out, size_t n, unsigned id)
{
    snprintf(out, n, "%s%04u", (id & 0xF0000000u) ? "ev" : "cf", (id & 0x0FFFFFFFu) % 10000u);
}

static int scene_exists(unsigned id)
{
    char path[256];
    char nm[16];
    scene_name(nm, sizeof nm, id);
    snprintf(path, sizeof path, "%s%s.txt", RES_getPath(), nm);
    return xglCdGetFileSize(path) > 0;
}

static long parse_scene(const char *v)
{
    if (!v || !*v)
        return -1;
    if ((v[0] == 'c' && v[1] == 'f') || (v[0] == 'C' && v[1] == 'F'))
        return strtol(v + 2, 0, 10);
    if ((v[0] == 'e' && v[1] == 'v') || (v[0] == 'E' && v[1] == 'V'))
        return 0x10000000L + strtol(v + 2, 0, 10);
    return strtol(v, 0, 0);
}

void xsg_room_tick(int kind, unsigned event_id)
{
    static int started;
    static unsigned last_event = ~0u;
    static const char *last_map = (const char *)-1;
    static long target = -1;
    static int called;

    const char *map = MapGetName(-1);
    if (event_id != last_event || map != last_map) {
        char nm[16];
        scene_name(nm, sizeof nm, event_id);
        xsg_logf("[room] vblank %u scene %s map %s\n", xsg_frame, nm, map ? map : "-");
        if (target >= 0 && event_id == (unsigned)target && !xsg_room_frame)
            xsg_room_frame = xsg_frame ? xsg_frame : 1;
        last_event = event_id;
        last_map = map;
    }

    if (!called && xsg_room_frame && kind == 2 && xsg_frame >= xsg_room_frame + ROOM_CALL_AT) {
        int ri = room_index();
        called = 1;
        if (ri >= 0 && rooms[ri].call == ROOM_SAVE) {
            xsg_logf("[room] vblank %u: file menu\n", xsg_frame);
            GameStateSave();
            MenuFileMain(0);
        } else if (ri >= 0 && rooms[ri].call >= ROOM_MINIGAME) {
            int n = rooms[ri].call - ROOM_MINIGAME;
            xsg_logf("[room] vblank %u: minigame %d\n", xsg_frame, n);
            Java_xeno_util_Runtime_minigameExec__I(0, &n);
            xsg_logf("[room] vblank %u: minigame %d returned\n", xsg_frame, n);
        } else if (ri >= 0 && rooms[ri].call == ROOM_ITEM_GET) {
            xsg_logf("[room] vblank %u: item get %d\n", xsg_frame, CreateEvtItemGetTask(0, 1));
        } else if (ri >= 0 && rooms[ri].call == ROOM_CHOICE) {
            void *menu = TMENU_create(-1);
            xsg_logf("[room] vblank %u: choice menu %p\n", xsg_frame, menu);
            if (menu) {
                TMENU_addItem(menu, "Save the game");
                TMENU_addItem(menu, "Keep exploring");
                TMENU_addItem(menu, "Check the U.M.N. terminal");
            }
        } else if (ri >= 0 && rooms[ri].call >= ROOM_SHOP && rooms[ri].call < ROOM_MINIGAME) {
            xsg_logf("[room] vblank %u: shop %d\n", xsg_frame, rooms[ri].call - ROOM_SHOP);
            GameStateSaveCameraLight();
            MenuShopMain(rooms[ri].call - ROOM_SHOP);
            GameStateRestoreCameraLight();
        }
        xsg_logf("[room] vblank %u: back in the field\n", xsg_frame);
    }

    if (started || kind != 2)
        return;
    started = 1;

    if (xsg_cfg_on("debug.scene_list")) {
        int n = 0;
        for (unsigned base = 0; base <= 0x10000000u; base += 0x10000000u)
            for (unsigned i = 0; i < 10000; ++i)
                if (scene_exists(base + i)) {
                    char nm[16];
                    scene_name(nm, sizeof nm, base + i);
                    xsg_logf("[room] exists %s\n", nm);
                    ++n;
                }
        xsg_logf("[room] %d scenes\n", n);
    }

    const char *jump = xsg_cfg("debug.jump");
    int ri = room_index();
    if (xsg_cfg_on("debug.give_all") || (ri >= 0 && rooms[ri].give))
        give_all();
    if (ri >= 0)
        jump = rooms[ri].scene;
    else if (ri == -2) {
        jump = 0;
        xsg_logf("[room] debug.room: no room \"%s\"; rooms:\n", xsg_cfg("debug.room"));
        for (size_t i = 0; i < NROOMS; ++i)
            xsg_logf("[room]   %-16s %s  %s\n", rooms[i].name, rooms[i].scene, rooms[i].what);
    }
    target = parse_scene(jump);
    if (target >= 0) {
        char nm[16];
        scene_name(nm, sizeof nm, (unsigned)target);
        if (!scene_exists((unsigned)target)) {
            xsg_logf("[room] debug.jump: %s not on the disc\n", nm);
            target = -1;
            return;
        }
        xsg_logf("[room] jump to %s\n", nm);
        {   /* [debug] skip_events = jump: fast through the boot's events, the jumped-to scene at speed */
            extern int xsg_skip_events;
            const char *se = xsg_cfg("debug.skip_events");
            if (se && !strcmp(se, "jump"))
                xsg_skip_events = 0;
        }
        Java_xeno_util_Runtime_jump_sub((int)target, -1);
    }
}

#include "common.h"
#include "shared.h"
#include "game_debug_menu.h"

static void PauseMenuPage0(void)
{
    int y;
    int i;

    if (PadData.half_2a & PAD_CROSS) {
        GameResourceDump(0);
    }

    xglFontDebugPrintf(136, 52, "WrkEnd:%8x", WorkEnd);

    y = 64;
    for (i = 0; i < 16; i++) {
        xglFontDebugPrintf(8, y, D_004D9EF8, actor[i].inUseId);
        y += 8;
    }

    GameResourceDump(1);
}

INCLUDE_ASM("asm/main/nonmatchings/game_debug_menu", PauseMenuPage2);

INCLUDE_ASM("asm/main/nonmatchings/game_debug_menu", PauseMenuPage3);

extern unsigned char GameLoopState[];
void PauseMenuPage2(void);
void PauseMenuPage3(void);
void PauseMenuPageUwamono(void);
void PauseMenuPageEnemy(void);
int PauseMenuPagePartyDebug(void);

void GameDebugMenu(void)
{
    int d;

    d = 0;
    if (PadData.half_2a & 4) {
        d = -1;
    }
    if (PadData.half_2a & 8) {
        d = 1;
    }
    if (d != 0) {
        GameLoopState[0x29F50] = (GameLoopState[0x29F50] + d + 6) % 6;
        GameLoopState[0x29F51] = 0;
    }
    switch (GameLoopState[0x29F50]) {
    case 0:
        PauseMenuPage3();
        break;
    case 1:
        PauseMenuPage0();
        break;
    case 2:
        PauseMenuPage2();
        break;
    case 3:
        PauseMenuPageUwamono();
        break;
    case 4:
        PauseMenuPageEnemy();
        break;
    case 5:
        if (PauseMenuPagePartyDebug()) {
        }
        break;
    }
}

/* Rebuilt UI: the menus (port/gpu/ui/menus.cpp). */
#pragma once
#include <cstdint>

struct ImDrawList;
struct ImFont;

namespace xsgui {

void MenuInit(void* shm_base);
/* A menu list was captured in the last few game vblanks (takes this frame's snapshot). */
bool MenuActive(uint32_t vsync);
/* The game's screen fade now: R G B, alpha << 24 (0x80 = full). */
uint32_t ScreenFade();
/* The list is a full menu or the pause screen: it shows the Settings hint (View / Back). */
bool MenuWantsSettings();
void MenuDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh);
/* Hold to skip (a cutscene or movie the game can skip, pointer.h PointerSkip): a button at the bottom
   right, [north] Hold to skip, filling as the hold goes.  Shows for a few seconds after any input. */
bool SkipActive();
void SkipDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh);

}  // namespace xsgui

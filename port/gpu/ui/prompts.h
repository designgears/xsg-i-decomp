/* Rebuilt UI: input prompt icons ("FREE Input Prompts Pack" by JulioCacko, CC0;
 * port/assets/prompts, vendored by port/tools/vendor_prompts.py).
 *
 * [display] prompts       auto | xbox | playstation | nintendo | keyboard
 *                         auto: the connected controller's family ([gamepad] layout), the
 *                         keyboard when no controller is connected.
 * [display] prompt_style  default | light | retro | alt | alt2 | drawn
 *                         drawn: the port's own vector glyphs, no icons. */
#pragma once
#include "imgui.h"

namespace xsgui {

enum PromptButton { kSouth, kEast, kWest, kNorth, kLB, kRB, kLT, kRT, kBack, kStart, kPromptCount };

/* Once per frame: whether a controller is connected (for auto). */
void PromptsFrame(bool have_pad);
/* The icon for a button in the current family and style (0: none, draw the fallback). */
ImTextureID PromptIcon(int button);
/* Draw it centred at c with height h; false when there is no icon. dim: unavailable. */
bool PromptDraw(ImDrawList* dl, int button, ImVec2 c, float h, float alpha);
/* The family in effect: 0 xbox, 1 playstation, 2 nintendo, 3 keyboard. */
int PromptFamily();

}  // namespace xsgui

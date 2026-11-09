/* "Enhanced original" UI ([display] ui = enhanced): the game draws its own 2D exactly
 * as on the PS2, except the characters of its font.  Each font glyph sprite is taken
 * out of the GS stream and drawn again over the picture at the same place, size and
 * colour from an upscaled copy of the game's own font texture: the original text,
 * sharp at any resolution.  See text_layer.cpp. */
#pragma once
#include <cstdint>

#include "imgui.h"

namespace ParallelGS { class GSInterface; }

namespace xsgtext {

void Init(ParallelGS::GSInterface* gs);
/* [display] ui = enhanced (re-read when the settings change). */
bool On();
/* One GIF message, before the GS sees it (it may be edited: glyph sprites are emptied). */
void Gif(uint32_t path, uint8_t* data, uint32_t len);
/* At each vsync: the glyphs of the frame just drawn become the ones shown. */
void Vsync();
/* Anything to draw this frame. */
bool Active();
/* Draws the shown glyphs over the game image at (gx, gy, gw, gh) (screen pixels). */
void Draw(ImDrawList* dl, float gx, float gy, float gw, float gh);

}  // namespace xsgtext

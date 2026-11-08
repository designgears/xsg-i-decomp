/* Controller button glyphs ([display] button_glyphs, port/docs/MENU.md).
 *
 * The game draws PlayStation button icons (dialog text, battle HUD, menus) from
 * small texture uploads.  This watches the GIF stream for host -> local image
 * transfers; when one completes whose content is a catalogued PlayStation icon,
 * it returns a GIF packet that uploads the replacement for the chosen controller
 * to the same place (sent right after, so every renderer sees it in order).
 *
 * debug.glyph_log = 1: every unique small upload is written to debug.glyph_dir
 * (PNG, indexed formats as grey levels) with its hash, for the catalogue. */
#pragma once
#include <cstdint>
#include <vector>

namespace ParallelGS { class GSInterface; }
namespace xsgglyph {

void Init();
/* Feed one GIF message (as sent to the GS).  Appends to `inject` the packets to
   send after it (path 2, PACKED A+D + IMAGE). */
void Feed(uint32_t path, const uint8_t* data, uint32_t len, std::vector<uint8_t>& inject);
/* The controller family the SDL layer sees: 0 none (keyboard), 1 PlayStation,
   2 Xbox, 3 Nintendo. */
void SetController(int family);
void SetGs(ParallelGS::GSInterface* gs);   // log mode: decodes icon rectangles from GS memory

}  // namespace xsgglyph

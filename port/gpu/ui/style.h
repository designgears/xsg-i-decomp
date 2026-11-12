/* Rebuilt UI: the shared look and motion (port/gpu/ui/).
 *
 * Layers, back to front: a soft drop shadow, the panel body (indigo gradient),
 * a sheen along the top edge, the frame, corner brackets.  Selection and the
 * mouse's hover: RowState, the settings menu's row look (static).
 * Anim() keeps a state per widget key; Spring / FadeIn / Tween are immediate
 * (no eased motion: it reads as input lag). */
#pragma once
#include <cstdint>

#include "imgui.h"

namespace xsgui {

/* The game UI's fonts, rasterised large (set by menu.cpp FontsUp): body text (Exo 2), labels and
   tags and numbers (Saira Semi Condensed ExtraBold: the original's tall, heavy, condensed small caps
   and rounded-square digits).  Any of
   them may be null before the first frame: callers fall back to the font they are given. */
enum UiFontKind { kFontBody, kFontBodyBold, kFontLabel, kFontNumber, kFontCount };
extern ImFont* g_ui_font[kFontCount];
inline ImFont* UiFont(UiFontKind k, ImFont* fallback) { return g_ui_font[k] ? g_ui_font[k] : fallback; }
/* Small caps the way the original sets its labels: the first letter of each word at full size,
   the rest uppercase at 78%.  Returns the width. */
float SmallCaps(ImDrawList* dl, ImFont* font, float size, ImVec2 p, ImU32 col, const char* s, float t, bool draw = true);
/* The y to draw text at so its capital letters are centred on `mid` (not the font's line box). */
float CapCentreY(ImFont* font, float size, float mid);
/* Text with a crisp dark outline (the original's style for numbers and labels). */
void OutlineText(ImDrawList* dl, ImFont* font, float size, ImVec2 p, ImU32 col, const char* s, float t);

/* Per-frame clock (seconds) and step; call once per frame before drawing. */
void AnimFrame(double now);
double AnimNow();

/* A keyed animation state: `seen` keeps it alive this frame. */
struct AnimState {
    float x = 0, y = 0, vx = 0, vy = 0;   // spring position / velocity (target set per frame)
    float a = 0;                          // fade 0..1
    float v = 0;                          // a tweened scalar (e.g. a gauge)
    double born = 0, last = 0;
    bool init = false;
};
/* The state for `key` (created on first use at (x, y), faded out). */
AnimState& Anim(uint64_t key, float x, float y);
/* Spring toward (tx, ty); returns the eased position. stiffness ~ 1/response time. */
ImVec2 Spring(AnimState& s, float tx, float ty, float stiffness = 18.0f);
/* Fade toward 1 (shown) at `rate` per second; returns the eased alpha. */
float FadeIn(AnimState& s, float rate = 7.0f);
/* Tween s.v toward `target` (exponential, `rate` per second). */
float Tween(AnimState& s, float target, float rate = 10.0f);
/* Drop states not touched for `ttl` seconds. */
void AnimGc(double ttl = 2.0);

/* Styled pieces.  `t`: an alpha 0..1 applied to everything. */
void Shadow(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, float t, float rounding = 0);
/* corners: bit 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right get a bracket (one
   covered by a header or plate gets none). */
void Panel(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, float t, bool accent = false, ImU32 tint = 0, int corners = 15);
/* The one selection / hover language, the settings menu's row (menu/option_rows.cpp RowFrame):
   sel: indigo fill, a cyan sweep from the left, a 1 px cyan edge and a 3 px left bar;
   hover (the mouse): the sweep and edge fainter, a 2 px bar; pressed: a cyan wash, full edge.
   Static: no sweep animation, no chevron.  col: the accent (kCyan; kTextOff for a disabled row). */
void RowState(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, float t, bool sel, bool hover, bool pressed, ImU32 col);
/* Text with a soft dark halo (readable over art). */
void HaloText(ImDrawList* dl, ImFont* font, float size, ImVec2 p, ImU32 col, const char* s, float t);

inline ImU32 Alpha(ImU32 c, float t) {
    const float a = float((c >> 24) & 0xff) * (t < 0 ? 0 : t > 1 ? 1 : t);
    return (c & 0x00ffffffu) | (ImU32(a) << 24);
}

}  // namespace xsgui

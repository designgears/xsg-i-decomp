/* Rebuilt UI: the shared look and motion (port/gpu/ui/).
 *
 * Layers, back to front: a soft drop shadow, the panel body (indigo gradient),
 * a sheen along the top edge, the frame, corner brackets.  Selection: a glow
 * bar with a slow light sweep and a cyan chevron that glides between rows.
 * Motion is keyed: every widget the game publishes has a stable key (its
 * address or slot), and Anim() keeps a spring / fade per key across frames, so
 * things ease in, glide, and fade out instead of popping. */
#pragma once
#include <cstdint>

#include "imgui.h"

namespace xsgui {

/* The game UI's fonts, rasterised large (set by menu.cpp FontsUp): body text (Exo 2), labels and
   tags (Michroma, the original's wide small caps), numbers (Orbitron, its square digits).  Any of
   them may be null before the first frame: callers fall back to the font they are given. */
enum UiFontKind { kFontBody, kFontBodyBold, kFontLabel, kFontNumber, kFontCount };
extern ImFont* g_ui_font[kFontCount];
inline ImFont* UiFont(UiFontKind k, ImFont* fallback) { return g_ui_font[k] ? g_ui_font[k] : fallback; }
/* Small caps the way the original sets its labels: the first letter of each word at full size,
   the rest uppercase at 78%.  Returns the width. */
float SmallCaps(ImDrawList* dl, ImFont* font, float size, ImVec2 p, ImU32 col, const char* s, float t, bool draw = true);
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
void GlowBar(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, float t, double now, ImU32 col);
void Chevron(ImDrawList* dl, ImVec2 tip, float size, float t, double now);
/* A thin rule with a bright head that sweeps once on open (age seconds since open). */
void SweepRule(ImDrawList* dl, ImVec2 a, ImVec2 b, float k, float t, float age, ImU32 col);
/* Text with a soft dark halo (readable over art). */
void HaloText(ImDrawList* dl, ImFont* font, float size, ImVec2 p, ImU32 col, const char* s, float t);

inline ImU32 Alpha(ImU32 c, float t) {
    const float a = float((c >> 24) & 0xff) * (t < 0 ? 0 : t > 1 ? 1 : t);
    return (c & 0x00ffffffu) | (ImU32(a) << 24);
}

}  // namespace xsgui

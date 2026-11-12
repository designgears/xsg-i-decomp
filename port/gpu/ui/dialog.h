/* Rebuilt UI: the dialog window ([display] ui = rebuilt).  The game publishes the
 * window (rectangle, speaker, typed-out lines, page state) in shared memory
 * (gpu_ipc.h GpuUiDialog); this draws it with Dear ImGui over the game image. */
#pragma once
#include <cstdint>
#include <string>

struct ImDrawList;
struct ImFont;

namespace xsgui {

void Init(void* shm_base);
/* The dialog was published within the last few game vblanks. */
bool DialogActive(uint32_t vsync);
/* Draw it into dl; (gx, gy, gw, gh): the game image's rectangle on screen. */
void DialogDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh, double now);
/* Battle party panels (GpuUiBattle): any published within the last few vblanks. */
bool BattleActive(uint32_t vsync);
void BattleDraw(ImDrawList* dl, ImFont* font, float gx, float gy, float gw, float gh, uint32_t vsync);
/* A controller face button, drawn for [gamepad] layout (xbox, playstation,
   nintendo) by position: 0 south (PS cross), 1 east (circle), 2 west (square),
   3 north (triangle).  c: the centre, r: the radius. */
void Glyph(ImDrawList* dl, ImFont* font, float cx, float cy, float r, int pos, bool dim);
/* A shoulder button: 0 left, 1 right, as "LB" / "L1" / "L" in a rounded tag. */
float ShoulderGlyph(ImDrawList* dl, ImFont* font, float x, float cy, float h, int side);
/* The one drawn keycap (no icon pack): a dark rounded square with the label; h tall, at x, centred
   on cy.  Returns the width. */
float KeyTag(ImDrawList* dl, ImFont* font, float x, float cy, float h, const char* label, float t);
/* UTF-8 text with a soft halo (style.h HaloText) where the PlayStation button symbols
   (U+25B3 triangle, U+25CB circle, U+25A1 square, U+00D7 cross) become controller
   glyphs (Glyph) for the layout.  Returns the width drawn. */
float RichText(ImDrawList* dl, ImFont* font, float size, float x, float y, unsigned col, const char* utf8, float t);
float RichTextWidth(ImFont* font, float size, const char* utf8);
/* The game's text encoding -> UTF-8 (exposed for tests). */
std::string Decode(const char* s);

}  // namespace xsgui

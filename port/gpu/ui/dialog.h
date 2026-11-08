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
/* The game's text encoding -> UTF-8 (exposed for tests). */
std::string Decode(const char* s);

}  // namespace xsgui

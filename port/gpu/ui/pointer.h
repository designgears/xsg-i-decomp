/* The native mouse, xsg-gpu's side (port/docs/MOUSE.md).
 *
 * The pointer goes to the game (gpu_ipc.h GpuMouse): its position in the game's 512 x 448
 * space under each layout anchor, its buttons and wheel, and the [mouse] options.  The
 * game's menus hit-test it against their own geometry and set their cursors directly
 * (port/runtime/mouse.cpp).  Where the rebuilt UI draws something in a layout of its own
 * (the title's rows, the battle command rows, the attack prompts) it reports what the
 * pointer is over here, as ui_kind / ui_index.  Coordinates are ImGui's (window logical
 * pixels). */
#pragma once
#include <string>
#include "imgui.h"
#include <cstdint>

namespace xsgui {

void PointerInit(void* shm_base);
/* Input (menu.cpp, while the settings menu is closed). button: SDL_BUTTON_* (1 left,
   2 middle, 3 right, 4 / 5 side). */
void PointerMove(float x, float y);
void PointerButton(int button, bool down);
void PointerWheel(float dy);
void PointerLeave();
/* The window lost the focus: the pointer is out and held buttons are released. */
void PointerFocusLost();
/* A key or gamepad button was pressed: the pointer hides until it moves again. */
void PointerHide();

/* Once per UI frame, before the rebuilt UI draws. */
void PointerBegin();
/* Something the rebuilt UI draws in its own layout: kind (GPU_MOUSE_UI_*), index. */
void PointerHit(ImVec2 a, ImVec2 b, int kind, int index);
/* A UI panel on screen: the 3D scene under it is not pickable (style.cpp Panel). */
void PointerBlock(ImVec2 a, ImVec2 b);
/* The top y for the rect (a, b) moved up clear of the panels drawn so far this frame (gap between). */
float PointerFreeAbove(ImVec2 a, ImVec2 b, float gap);
/* A clickable area that opens the settings menu (the "Settings" hints). */
void PointerSettings(ImVec2 a, ImVec2 b);
/* After the draws: publishes the pointer.  (gx, gy, gw, gh): the game picture on screen;
   rebuilt: the rebuilt UI's layout maps the anchors; blocked: the settings menu is open.
   Returns true when the settings menu should open.  dl: unused (the hover state is drawn by
   the elements themselves, PointerHovered). */
bool PointerEnd(ImDrawList* dl, float gx, float gy, float gw, float gh, bool rebuilt, bool blocked);
/* Whether the OS pointer should show. */
bool PointerVisible();
/* The pointer is over something clickable (the hand cursor). */
bool PointerHot();
/* Hold to skip (gpu_ipc.h GpuMouse g_skip): bits 0-7 progress, bit 8 a skippable scene, bit 9 show. */
uint32_t PointerSkip();
/* The movie caption the game asks for now (GpuMouse g_caption; UTF-8, '\n' between lines), "" none. */
const std::string& PointerCaption();
/* The native renderer (native.cpp): the pointer in the game's 512 x 448 frame (x may lie in the
   widescreen margins, < 0 or >= 512; false when the mouse is off, outside, or over a UI panel), the object ids the game wants outlined, and the object id it found
   under the pointer (-1 unknown: no native frame). */
bool PointerGame(int* x, int* y);
void PointerOutlineIds(int* selected, int* hovered);
void PointerSetPick(int id);
/* Actors (bit = index) the renderer outlines when they are under the pointer, without waiting for the game. */
void PointerHoverable(uint32_t* mask8);
/* Last frame's report: the pointer was over the hit (kind, index); *pressed: with the left
   button held.  The rebuilt UI draws its hover / press state from this (style.h RowState). */
bool PointerHovered(int kind, int index, bool* pressed);

}  // namespace xsgui

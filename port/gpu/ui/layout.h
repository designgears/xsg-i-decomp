/* Rebuilt UI: layout from the game's 512 x 448 coordinates to the screen, ready for a
 * native widescreen mode.
 *
 * Every element is placed with an anchor: LEFT keeps its distance from the left edge,
 * RIGHT from the right edge, CENTRE from the centre, STRETCH spans (its left part
 * follows the left edge, its right part the right).  Proportions are the original 4:3
 * display's (a PS2 pixel is 7/6 as wide as tall), scaled by the game image's height.
 *
 * [display] ui_layout:
 *   game (default)  the UI area is the game image: identical to the original layout.
 *   wide            the UI area is the whole window (less a margin): left-anchored UI
 *                   moves out to the window's left edge, right-anchored to its right
 *                   edge.  With a 4:3 picture that previews a widescreen layout; a native
 *                   widescreen picture will use the same anchors.
 * [display] widescreen = 1 (native renderer): the game area is the 16:9 picture's centre 4:3
 * and edge UI is anchored to the picture's edges, whatever ui_layout says. */
#pragma once
#include "imgui.h"

namespace xsgui {

enum Anchor { kLeft = -1, kCentre = 0, kRight = 1, kStretch = 2 };

/* Once per frame: the game image's rectangle (screen pixels). */
void LayoutBegin(float gx, float gy, float gw, float gh);
/* PS2 point -> screen, under an anchor. */
ImVec2 Map(float x, float y, int anchor);
/* Screen -> PS2 point under an anchor (the inverse of Map; kStretch: as kLeft). */
ImVec2 Unmap(ImVec2 screen, int anchor);
/* The anchor for an element spanning PS2 x0..x1. */
int AnchorFor(float x0, float x1);
/* Horizontal / vertical size of one PS2 pixel on screen. */
float LayoutKx();
float LayoutK();
/* The UI area (screen pixels): the clip rectangle for the UI. */
ImVec4 LayoutArea();

}  // namespace xsgui

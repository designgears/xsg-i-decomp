/*
 * Two-pane settings pages for the in-game menu: option rows on the left, the
 * selected (keyboard / gamepad) or hovered (mouse) row's description on the
 * right.  Each row shows a setting of the cfg table (cfg.h) and changes it
 * through xsgcfg::Set: Enter / A / a click steps it, Left / Right step it.  A
 * row marks the default value, says "after restart" when the setting is read
 * at start, and is greyed out with a note when another setting makes it
 * meaningless (opt_disable_next).
 *
 * Adapted from TotK Recomp's runtime/ui/option_rows.{h,cpp} (MIT, Copyright
 * (c) 2026 Valiantsin Dzerakh; itself adapted from SavageWindRecomp, MIT):
 * the two panes, the row model, left / right stepping with hold speed-up and
 * the binding cells.  Text is plain strings here, and the look is ours.
 */
#ifndef XSG_MENU_OPTION_ROWS_H
#define XSG_MENU_OPTION_ROWS_H

#include "imgui.h"

namespace xsgmenu {

/* Theme colours (menu.cpp sets the ImGui style from the same values). */
namespace col {
constexpr ImU32 kPanel = IM_COL32(20, 19, 52, 225);       // the minimap's indigo
constexpr ImU32 kPanelHi = IM_COL32(40, 40, 96, 230);
constexpr ImU32 kGrid = IM_COL32(110, 120, 200, 26);
constexpr ImU32 kFrame = IM_COL32(150, 180, 235, 255);     // the minimap frame's periwinkle
constexpr ImU32 kFrameDim = IM_COL32(150, 180, 235, 70);
constexpr ImU32 kCyan = IM_COL32(118, 214, 255, 255);      // selection / accent
constexpr ImU32 kCyanDim = IM_COL32(118, 214, 255, 90);
constexpr ImU32 kText = IM_COL32(232, 238, 252, 255);
constexpr ImU32 kTextDim = IM_COL32(150, 162, 205, 255);
constexpr ImU32 kTextOff = IM_COL32(100, 106, 140, 255);
constexpr ImU32 kSteel = IM_COL32(113, 145, 153, 255);     // the title screen's steel blue
constexpr ImU32 kWarn = IM_COL32(244, 196, 98, 255);
}  // namespace col

/* Corner-bracket accents on a rectangle (len: bracket arm length). */
void DrawBrackets(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c, float len, float thick);

void opt_begin(const char* id, float scale);
void opt_end();
void opt_reserve_bottom(float h);
void opt_focus_first();
/* The next row is greyed out (note under its label and in its description). */
void opt_disable_next(const char* note);

void opt_group(const char* title);
void opt_text(const char* text, bool warn = false);

/* names: one per value of the key's "a|b|c" list (nullptr: the values). */
bool opt_toggle(const char* key, const char* label, const char* desc);
bool opt_choice(const char* key, const char* label, const char* const* names, const char* desc);
/* zero: shown in place of the value when it is 0 ("Never"). */
bool opt_slider(const char* key, const char* label, int lo, int hi, int step, const char* fmt, const char* desc, const char* zero = nullptr);
/* A row cycling through `names` that is not one setting (`id` only identifies the row):
   *cur is the shown entry (-1: `none` is shown); returns true and sets *cur when changed. */
bool opt_list(const char* id, const char* label, const char* const* names, int n, int* cur, const char* none, const char* desc);
/* A free-text row (paths, lists, numbers): Enter / A / a click opens a text
   field, Enter keeps it, Esc cancels. */
bool opt_edit(const char* key, const char* label, const char* desc);
/* A text field is open (the menu keeps Esc / B for it). */
bool opt_editing();
/* The row that fits the key's value list: toggle, choice, slider or text. */
bool opt_auto(const char* key, const char* label, const char* desc);
/* An action row; true on activation.  value: shown on the right. */
bool opt_button(const char* id, const char* label, const char* desc, const char* value = "");
/* A read-only row. */
void opt_info(const char* id, const char* label, const char* value, const char* desc);
/* Binding row: label + three cells; returns the cell activated (0..2), 10 + cell cleared (Delete / Backspace /
   gamepad X), or -1. */
int opt_bind_row(const char* id, const char* label, const char* desc, const char* const cells[3], const bool enabled[3]);
void opt_bind_header(const char* c0, const char* c1, const char* c2);

}  // namespace xsgmenu

#endif

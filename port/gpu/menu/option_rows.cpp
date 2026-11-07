/*
 * Two-pane settings pages (option_rows.h).
 *
 * Adapted from TotK Recomp's runtime/ui/option_rows.cpp
 * (https://github.com/valentderah/totk-pc-recomp, MIT, Copyright (c) 2026
 * Valiantsin Dzerakh), which adapts SavageWindRecomp's option_rows.mm (MIT):
 * the two panes, the row model (default mark, "after restart", greyed out by
 * the environment), left / right stepping with the hold speed-up and the
 * binding cells.  Changes: plain strings instead of a text catalog, the
 * xsgcfg table, and a Xenosaga-style look (thin cyan outlines, corner
 * brackets, indigo panels).
 */
#include "option_rows.h"

#include "cfg.h"

#include "imgui_internal.h"   // SetFocusID, SetKeyOwner, NavCursorVisible

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace xsgmenu {
namespace {

float g_scale = 1;
bool g_focus_req, g_take_first;
bool g_held;
float g_bottom;
const char* g_disable_note;
struct Desc { std::string text; };
Desc g_hover, g_focus, g_first, g_shown;

void desc_set(Desc& d, const char* desc, const char* key, const char* disabled) {
    d.text = desc ? desc : "";
    const xsgcfg::Meta* m = key ? xsgcfg::Find(key) : nullptr;
    if (disabled) {
        d.text += "\n\n";
        d.text += disabled;
    } else if (m && xsgcfg::FromEnv(key)) {
        d.text += "\n\nFixed by the environment variable " + xsgcfg::EnvName(key) + ". Unset it to change this here.";
    } else if (m && !m->live) {
        d.text += "\n\nApplies after a restart (Restart now, below).";
    }
    if (m) {
        d.text += "\n\nsettings.ini   ";
        d.text += m->key;
        if (m->env) { d.text += "\nenvironment   "; d.text += m->env; }
    }
}

struct RowView {
    const char* value;
    bool is_default;
    bool arrows;
    const char* note;
};

void FitText(ImDrawList* dl, float x0, float x1, float y, float h, ImU32 c, const char* t, bool centred) {
    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize(), room = x1 - x0, tw = font->CalcTextSizeA(fs, FLT_MAX, 0, t).x;
    if (tw > room) {
        fs = std::fmax(fs * room / tw, fs * 0.65f);
        tw = font->CalcTextSizeA(fs, FLT_MAX, 0, t).x;
    }
    float x = centred && tw < room ? x0 + (room - tw) * 0.5f : x0;
    ImVec4 clip(x0, y, x1, y + h);
    dl->AddText(font, fs, ImVec2(x, y + (h - fs) * 0.5f), c, t, nullptr, 0, &clip);
}

/* The row frame: a dark translucent bar, a thin outline, brackets and a left tick when selected. */
void RowFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, bool sel, bool off) {
    dl->AddRectFilled(a, b, sel ? IM_COL32(60, 76, 150, 90) : IM_COL32(8, 8, 30, 80));
    if (sel) {
        // a faint gradient sweep from the left, like the game's selection bars
        dl->AddRectFilledMultiColor(a, ImVec2(a.x + (b.x - a.x) * 0.6f, b.y), IM_COL32(118, 214, 255, 46),
                                    IM_COL32(118, 214, 255, 0), IM_COL32(118, 214, 255, 0), IM_COL32(118, 214, 255, 46));
        dl->AddRect(a, b, off ? col::kCyanDim : col::kCyan, 0, 0, 1.0f);
        DrawBrackets(dl, ImVec2(a.x - 3 * g_scale, a.y - 3 * g_scale), ImVec2(b.x + 3 * g_scale, b.y + 3 * g_scale),
                     col::kCyan, 8 * g_scale, 1.5f * g_scale);
        dl->AddRectFilled(a, ImVec2(a.x + 3 * g_scale, b.y), col::kCyan);
    } else {
        dl->AddRect(a, b, IM_COL32(150, 180, 235, off ? 22 : 45), 0, 0, 1.0f);
    }
}

void TakeFocus() {
    if (!g_take_first) return;
    ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
    ImGui::SetNavCursorVisible(true);
    ImGui::SetScrollY(0);
    g_take_first = false;
}

/* One row; returns 2 (activate), -1 / +1 (left / right) or 0 (always 0 when disabled). */
int Row(const char* key, const char* label, const char* desc, const RowView& v) {
    const xsgcfg::Meta* m = xsgcfg::Find(key);
    const char* disabled = g_disable_note;
    g_disable_note = nullptr;
    bool off = disabled || (m && xsgcfg::FromEnv(key));
    ImGui::PushID(key);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float fs = ImGui::GetFontSize(), w = ImGui::GetContentRegionAvail().x - 6 * g_scale, h = std::floor(fs * 2.15f);
    p.x += 3 * g_scale;
    ImGui::SetCursorScreenPos(p);
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
    bool clicked = ImGui::InvisibleButton("##row", ImVec2(w, h), ImGuiButtonFlags_EnableNav);
    ImGui::PopStyleColor();
    TakeFocus();
    bool hov = ImGui::IsItemHovered(), foc = ImGui::IsItemFocused() && GImGui->NavCursorVisible;
    int step = clicked && !off ? 2 : 0;
    g_held = false;
    if (foc && !off) {
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) step = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) || ImGui::IsKeyPressed(ImGuiKey_RightArrow)) step = 1;
        static const ImGuiKey steps[] = {ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight, ImGuiKey_LeftArrow,
                                         ImGuiKey_RightArrow};
        for (ImGuiKey k : steps) {
            if (ImGui::GetKeyData(k)->DownDuration > 0.8f) g_held = true;
            ImGui::SetKeyOwner(k, ImGui::GetItemID());   // left / right step the row, not navigate
        }
    }
    if (foc) desc_set(g_focus, desc, m ? key : nullptr, disabled);
    if (hov) desc_set(g_hover, desc, m ? key : nullptr, disabled);
    if (g_first.text.empty()) desc_set(g_first, desc, m ? key : nullptr, disabled);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const bool sel = hov || foc;
    RowFrame(dl, p, ImVec2(p.x + w, p.y + h), sel, off);
    float pad = 16 * g_scale, small = fs * 0.70f;
    float ty = p.y + fs * 0.22f, ny = p.y + fs * 1.22f;
    float right = p.x + w - pad, value_left = right;
    if (v.arrows && v.value[0]) {
        float aw = fs * 0.36f, gap = fs * 0.5f, cy = ty + fs * 0.52f;
        float rx = right - aw, lx = rx - gap - ImGui::CalcTextSize(v.value).x - gap - aw;
        ImU32 ac = off ? col::kTextOff : sel ? col::kCyan : col::kFrameDim;
        dl->AddTriangleFilled(ImVec2(lx, cy), ImVec2(lx + aw, cy - aw * 0.8f), ImVec2(lx + aw, cy + aw * 0.8f), ac);
        dl->AddTriangleFilled(ImVec2(rx + aw, cy), ImVec2(rx, cy - aw * 0.8f), ImVec2(rx, cy + aw * 0.8f), ac);
        if (clicked && !off) {
            float mx = ImGui::GetIO().MousePos.x;
            if (mx >= lx - gap && mx <= lx + aw + gap) step = -1;
            else if (mx >= rx - gap && mx <= rx + aw + gap) step = 1;
        }
        right = rx - gap;
        value_left = lx;
    }
    if (v.value[0]) {
        float vx = right - ImGui::CalcTextSize(v.value).x;
        dl->AddText(ImVec2(vx, ty), off ? col::kTextOff : v.is_default ? col::kTextDim : col::kText, v.value);
        if (!v.arrows) value_left = vx;
    }
    if (v.note || v.is_default) {
        const char* t = v.note ? v.note : "DEFAULT";
        float tw = font->CalcTextSizeA(small, FLT_MAX, 0, t).x;
        dl->AddText(font, small, ImVec2(right - tw, ny), v.note ? col::kWarn : col::kSteel, t);
    }
    ImVec4 clip(p.x, p.y, value_left - fs * 0.6f, p.y + h);
    dl->AddText(font, fs, ImVec2(p.x + pad, ty), off ? col::kTextOff : col::kText, label, nullptr, 0, &clip);
    const char* note = disabled ? disabled
                       : off    ? "SET BY ENVIRONMENT"
                       : m && !m->live ? "AFTER RESTART"
                                       : nullptr;
    if (note) dl->AddText(font, small, ImVec2(p.x + pad, ny), off ? col::kWarn : col::kSteel, note);
    ImGui::PopID();
    return off ? 0 : step;
}

std::vector<std::string> Values(const xsgcfg::Meta* m) {
    std::vector<std::string> r;
    if (!m || !m->values) return r;
    std::string s = m->values;
    size_t i = 0;
    while (i <= s.size()) {
        size_t j = s.find('|', i);
        if (j == std::string::npos) j = s.size();
        r.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    return r;
}

}  // namespace

void DrawBrackets(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c, float len, float t) {
    dl->AddLine(a, ImVec2(a.x + len, a.y), c, t);
    dl->AddLine(a, ImVec2(a.x, a.y + len), c, t);
    dl->AddLine(ImVec2(b.x, a.y), ImVec2(b.x - len, a.y), c, t);
    dl->AddLine(ImVec2(b.x, a.y), ImVec2(b.x, a.y + len), c, t);
    dl->AddLine(ImVec2(a.x, b.y), ImVec2(a.x + len, b.y), c, t);
    dl->AddLine(ImVec2(a.x, b.y), ImVec2(a.x, b.y - len), c, t);
    dl->AddLine(b, ImVec2(b.x - len, b.y), c, t);
    dl->AddLine(b, ImVec2(b.x, b.y - len), c, t);
}

void opt_focus_first() { g_focus_req = true; }
void opt_reserve_bottom(float h) { g_bottom = h; }
void opt_disable_next(const char* note) { g_disable_note = note; }

void opt_begin(const char* id, float scale) {
    g_scale = scale;
    g_take_first = g_focus_req;
    g_focus_req = false;
    if (g_take_first) ImGui::NavMoveRequestCancel();
    g_hover.text.clear();
    g_focus.text.clear();
    g_first.text.clear();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.y -= g_bottom;
    ImGui::PushID(id);
    ImGui::BeginChild("##rows", ImVec2(std::floor(avail.x * 0.60f), avail.y), ImGuiChildFlags_NavFlattened);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 6 * scale));
    ImGui::Dummy(ImVec2(0, 2 * scale));
}

void opt_end() {
    ImGui::PopStyleVar();
    ImGui::EndChild();
    if (!g_hover.text.empty()) g_shown = g_hover;
    else if (!g_focus.text.empty()) g_shown = g_focus;
    else if (g_shown.text.empty()) g_shown = g_first;
    ImGui::SameLine(0, 0);
    ImVec2 p = ImGui::GetCursorScreenPos(), avail = ImGui::GetContentRegionAvail();
    avail.y -= g_bottom;
    float gap = 22 * g_scale;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // the description pane: a darker inset panel with a periwinkle frame and brackets
    ImVec2 a(p.x + gap, p.y), b(p.x + avail.x, p.y + avail.y);
    dl->AddRectFilled(a, b, IM_COL32(10, 10, 34, 150));
    dl->AddRect(a, b, col::kFrameDim);
    DrawBrackets(dl, a, b, col::kFrame, 12 * g_scale, 1.5f * g_scale);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.72f, ImVec2(a.x + 14 * g_scale, a.y + 8 * g_scale),
                col::kSteel, "DESCRIPTION");
    ImGui::SetCursorScreenPos(ImVec2(a.x, a.y + 30 * g_scale));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14 * g_scale, 6 * g_scale));
    ImGui::BeginChild("##desc", ImVec2(b.x - a.x, avail.y - 32 * g_scale), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(g_shown.text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::PopID();
}

void opt_group(const char* title) {
    ImGui::Dummy(ImVec2(0, 4 * g_scale));
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x, fs = ImGui::GetFontSize() * 0.8f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* f = ImGui::GetFont();
    dl->AddRectFilled(ImVec2(p.x + 3 * g_scale, p.y + fs * 0.25f), ImVec2(p.x + 3 * g_scale + fs * 0.5f, p.y + fs * 0.75f), col::kCyan);
    dl->AddText(f, fs, ImVec2(p.x + 3 * g_scale + fs * 0.9f, p.y), col::kCyan, title);
    float tw = f->CalcTextSizeA(fs, FLT_MAX, 0, title).x;
    dl->AddLine(ImVec2(p.x + 3 * g_scale + fs * 1.4f + tw, p.y + fs * 0.55f), ImVec2(p.x + w - 3 * g_scale, p.y + fs * 0.55f),
                col::kFrameDim);
    ImGui::Dummy(ImVec2(w, fs * 1.15f));
}

void opt_text(const char* text, bool warn) {
    ImGui::PushTextWrapPos(0);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(warn ? col::kWarn : col::kTextDim));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
}

bool opt_toggle(const char* key, const char* label, const char* desc) {
    const xsgcfg::Meta* m = xsgcfg::Find(key);
    bool on = xsgcfg::GetBool(key), def = m && std::strcmp(m->def, "0") != 0;
    RowView v = {on ? "ON" : "OFF", on == def, false, nullptr};
    if (!Row(key, label, desc, v)) return false;
    xsgcfg::Set(key, on ? "0" : "1");
    return true;
}

bool opt_choice(const char* key, const char* label, const char* const* names, const char* desc) {
    const xsgcfg::Meta* m = xsgcfg::Find(key);
    std::vector<std::string> vals = Values(m);
    const int n = int(vals.size());
    std::string now = xsgcfg::Get(key);
    int cur = -1, def = -1;
    for (int i = 0; i < n; ++i) {
        if (vals[i] == now) cur = i;
        if (m && vals[i] == m->def) def = i;
    }
    const char* shown = cur >= 0 ? (names ? names[cur] : vals[cur].c_str()) : now.c_str();
    RowView v = {shown, cur == def, true, nullptr};
    int step = Row(key, label, desc, v);
    if (!step || n == 0) return false;
    int nv = cur < 0 ? 0 : step == 2 ? (cur + 1) % n : std::max(0, std::min(n - 1, cur + step));
    if (nv == cur) return false;
    xsgcfg::Set(key, vals[nv]);
    return true;
}

bool opt_slider(const char* key, const char* label, int lo, int hi, int step, const char* fmt, const char* desc) {
    const xsgcfg::Meta* m = xsgcfg::Find(key);
    int v = xsgcfg::GetInt(key), def = m ? std::atoi(m->def) : lo;
    char shown[48];
    std::snprintf(shown, sizeof shown, fmt, v);
    RowView rv = {shown, v == def, true, nullptr};
    // a thin bar under the value: the level
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x, fs = ImGui::GetFontSize(), h = std::floor(fs * 2.15f);
    int s = Row(key, label, desc, rv);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float x0 = p.x + 18 * g_scale, x1 = p.x + w * 0.45f, y = p.y + h - 4 * g_scale;
        float t = hi > lo ? float(v - lo) / float(hi - lo) : 0;
        dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + 2 * g_scale), IM_COL32(150, 180, 235, 40));
        dl->AddRectFilled(ImVec2(x0, y), ImVec2(x0 + (x1 - x0) * t, y + 2 * g_scale), xsgcfg::FromEnv(key) ? col::kTextOff : col::kCyan);
    }
    if (!s) return false;
    int steps = g_held && (hi - lo) > 40 * step ? 5 : 1;
    int n = (v / step) * step + (s == 2 ? step : s * step * steps);
    if (s == 2 && n > hi) n = lo;
    n = std::max(lo, std::min(hi, n));
    if (n == v) return false;
    xsgcfg::Set(key, std::to_string(n));
    return true;
}

bool opt_button(const char* id, const char* label, const char* desc, const char* value) {
    RowView v = {value ? value : "", false, false, nullptr};
    return Row(id, label, desc, v) == 2;
}

void opt_info(const char* id, const char* label, const char* value, const char* desc) {
    RowView v = {value ? value : "", false, false, nullptr};
    Row(id, label, desc, v);
}

namespace {
void BindColumns(float x, float w, float cx[3], float* cw) {
    float lw = std::floor(w * 0.30f), gap = std::floor(8 * g_scale);
    *cw = std::floor((w - lw - gap * 3) / 3);
    for (int c = 0; c < 3; c++) cx[c] = x + lw + gap + c * (*cw + gap);
}
}  // namespace

void opt_bind_header(const char* c0, const char* c1, const char* c2) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x, cx[3], cw, fs = ImGui::GetFontSize() * 0.75f;
    BindColumns(p.x, w, cx, &cw);
    const char* cols[3] = {c0, c1, c2};
    ImFont* font = ImGui::GetFont();
    for (int c = 0; c < 3; c++) {
        float tw = font->CalcTextSizeA(fs, FLT_MAX, 0, cols[c]).x;
        ImGui::GetWindowDrawList()->AddText(font, fs, ImVec2(cx[c] + (cw - tw) * 0.5f, p.y), col::kSteel, cols[c]);
    }
    ImGui::Dummy(ImVec2(w, fs * 1.2f));
}

int opt_bind_row(const char* id, const char* label, const char* desc, const char* const cells[3], const bool enabled[3]) {
    ImGui::PushID(id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float fs = ImGui::GetFontSize(), w = ImGui::GetContentRegionAvail().x, h = std::floor(fs * 1.75f), cx[3], cw;
    BindColumns(p.x, w, cx, &cw);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float pad = 6 * g_scale;
    FitText(dl, p.x + 12 * g_scale, cx[0] - fs * 0.3f, p.y, h, col::kText, label, false);
    int result = -1;
    bool row_foc = false, row_hov = false;
    for (int c = 0; c < 3; c++) {
        ImVec2 a(cx[c], p.y), b(cx[c] + cw, p.y + h);
        const char* t = cells[c] && cells[c][0] ? cells[c] : "-";
        if (!enabled[c]) {
            dl->AddRect(a, b, IM_COL32(150, 180, 235, 18));
            FitText(dl, a.x + pad, b.x - pad, a.y, h, col::kTextOff, t, true);
            continue;
        }
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(c);
        ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
        bool clicked = ImGui::InvisibleButton("##cell", ImVec2(cw, h), ImGuiButtonFlags_EnableNav);
        ImGui::PopStyleColor();
        TakeFocus();
        bool hov = ImGui::IsItemHovered(), foc = ImGui::IsItemFocused() && GImGui->NavCursorVisible;
        row_foc |= foc;
        row_hov |= hov;
        if (clicked) result = c;
        if (foc && (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false) ||
                    ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false)))
            result = 10 + c;
        RowFrame(dl, a, b, hov || foc, false);
        FitText(dl, a.x + pad, b.x - pad, a.y, h, std::strcmp(t, "-") ? col::kText : col::kTextOff, t, true);
        ImGui::PopID();
    }
    if (row_foc) desc_set(g_focus, desc, nullptr, nullptr);
    if (row_hov) desc_set(g_hover, desc, nullptr, nullptr);
    if (g_first.text.empty()) desc_set(g_first, desc, nullptr, nullptr);
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(w, h));
    ImGui::PopID();
    return result;
}

}  // namespace xsgmenu

/*
 * The in-game menu (menu.h, port/docs/MENU.md).
 *
 * Modelled on TotK Recomp's runtime/ui/menu.cpp and menu_sdl.cpp
 * (https://github.com/valentderah/totk-pc-recomp, MIT, Copyright (c) 2026
 * Valiantsin Dzerakh): open / close keys and the gamepad combo, the game
 * kept running with neutral input, gamepad navigation with the stick mapped
 * to the D-pad with hysteresis, tabs on LB / RB, the two-pane option pages,
 * the debounced save and save on close, the F10 overlay (fps, then a full
 * panel with 1% / 0.1% lows and a frame-time graph) and the per-tab
 * screenshot test hook.  The look (indigo "virtual space" panels, thin cyan
 * outlines, corner brackets, grid backdrop) follows Xenosaga's own UI.
 */
#include "menu.h"

#include "cfg.h"
#include "menu_render.h"
#include "option_rows.h"
#include "../xsg_input.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "../ui/dialog.h"
#include "../ui/style.h"
#include "../ui/art.h"
#include "../ui/menus.h"
#include "../ui/prompts.h"
#ifdef XSG_GPU_SDL
#include "imgui_impl_sdl3.h"
#include <SDL3/SDL.h>
#endif

#include "command_buffer.hpp"
#include "device.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

extern char** environ;

#ifndef XSG_BUILD_VERSION
#define XSG_BUILD_VERSION "unknown"
#endif

namespace xsgmenu {
namespace {

const unsigned char kFontBody[] = {
#include "Rajdhani-Medium.ttf.inc"
};
const unsigned char kFontHead[] = {
#include "Rajdhani-SemiBold.ttf.inc"
};
const unsigned char kGameBody[] = {
#include "Exo2-Medium.ttf.inc"
};
const unsigned char kGameBold[] = {
#include "Exo2-Bold.ttf.inc"   // the original's heavy game text
};
const unsigned char kGameLabel[] = {
#include "Michroma-Regular.ttf.inc"
};
const unsigned char kGameNumber[] = {
#include "Orbitron-SemiBold.ttf.inc"
};

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[xsg-gpu] menu: %s\n", b);
}

double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Vulkan::Device* g_dev;
GpuSettings* g_shared;
Host g_host;
bool g_ok;
#ifdef XSG_GPU_SDL
SDL_Window* g_win;
#endif

bool g_open;
bool g_visible;           // the last Frame() laid out something to draw
double g_open_t;          // when it opened (fade-in)
float g_fade;             // 0..1 this frame
bool g_reset;             // opened / closed since the last layout: drop stale input
int g_tab, g_tab_req = -1;
int g_overlay;            // 0 off, 1 fps, 2 full
unsigned g_seen_ver;
float g_font_scale;       // scale the atlas was built for
ImFont* g_head;
ImFont* g_small;
Stats g_st;
std::string g_shot_note;
double g_shot_note_t;

// restart-pending: the after-restart settings as they were at start
std::vector<std::pair<std::string, std::string>> g_boot_values;

enum { TAB_DISPLAY, TAB_RENDERER, TAB_GAME, TAB_CONTROLS, TAB_INFO, TAB_DEBUG, TAB_COUNT };
const char* const kTabNames[TAB_COUNT] = {"DISPLAY", "RENDERER", "GAME", "CONTROLS", "INFO", "DEBUG"};

/* ------------------------------------------------------------- perf overlay */
std::vector<float> g_ft;      // frame times (ms), last 2048 presents
size_t g_ft_pos;
double g_last_frame_t;
double g_fps_t0;
int g_fps_n;
float g_fps_shown;

void PerfTick() {
    double t = Now();
    if (g_last_frame_t > 0) {
        float ms = float((t - g_last_frame_t) * 1000.0);
        if (ms < 2000) {
            if (g_ft.size() < 2048) g_ft.push_back(ms);
            else g_ft[g_ft_pos] = ms;
            g_ft_pos = (g_ft_pos + 1) % 2048;
        }
    }
    g_last_frame_t = t;
    g_fps_n++;
    if (t - g_fps_t0 >= 0.5) {
        g_fps_shown = float(g_fps_n / (t - g_fps_t0));
        g_fps_t0 = t;
        g_fps_n = 0;
    }
}

/* ------------------------------------------------------------- gamepad input */
uint32_t g_pad_prev;
int g_stick_dir = -1;   // ImGuiKey of the D-pad direction the stick holds, -1 none
bool g_combo_prev;

/* Past half way the stick holds the D-pad direction of its larger axis, kept
   until that axis falls under 0.35 (hysteresis, as TotK Recomp's menu.cpp). */
int StickToDpad(int lx, int ly) {
    float x = lx / 32767.0f, y = -ly / 32767.0f;   // y up
    float along = g_stick_dir == ImGuiKey_GamepadDpadLeft ? -x : g_stick_dir == ImGuiKey_GamepadDpadRight ? x
                : g_stick_dir == ImGuiKey_GamepadDpadUp ? y : g_stick_dir == ImGuiKey_GamepadDpadDown ? -y : 0;
    if (g_stick_dir >= 0 && along > 0.35f) return g_stick_dir;
    if (std::fmax(std::fabs(x), std::fabs(y)) < 0.5f) return -1;
    return std::fabs(x) > std::fabs(y) ? (x < 0 ? ImGuiKey_GamepadDpadLeft : ImGuiKey_GamepadDpadRight)
                                       : (y > 0 ? ImGuiKey_GamepadDpadUp : ImGuiKey_GamepadDpadDown);
}

/* ------------------------------------------------------------- binds capture */
enum { CAP_NONE, CAP_KEY, CAP_PAD };
int g_cap;            // waiting for a press
uint32_t g_cap_mask;  // the action
int g_cap_old;        // the binding the cell had (-1 none)
double g_cap_t;
uint32_t g_cap_pad_prev;

/* ------------------------------------------------------------- test hook */
struct TestSet { uint32_t vsync; std::vector<std::pair<std::string, std::string>> kv; };
struct {
    uint32_t at = 0, per = 30, w = 1280, h = 960;
    int page = -1;          // the page being shown, -1 none
    uint32_t page_t = 0;
    std::string dir = ".";
    std::vector<TestSet> sets;
    size_t next_set = 0;
    int saved_overlay = 0;
    std::string shot;
} g_test;

void TestParse() {
    if (const char* s = xsgcfg::CStr("debug.menu_test"); s && *s) {
        unsigned a = 0, p = 0, w = 0, h = 0;
        int n = std::sscanf(s, "%u:%u:%ux%u", &a, &p, &w, &h);
        if (n >= 1) g_test.at = a ? a : 1;
        if (n >= 2 && p) g_test.per = p;
        if (n >= 4 && w && h) { g_test.w = w; g_test.h = h; }
        if (const char* d = xsgcfg::CStr("debug.menu_test_dir"); d && *d) g_test.dir = d;
        Log("test hook: open at vsync %u, %u vsyncs per page, %ux%u -> %s", g_test.at, g_test.per, g_test.w, g_test.h,
            g_test.dir.c_str());
    }
    if (const char* s = xsgcfg::CStr("debug.menu_test_set"); s && *s) {
        std::string all = s;
        size_t i = 0;
        while (i < all.size()) {
            size_t j = all.find(';', i);
            if (j == std::string::npos) j = all.size();
            std::string e = all.substr(i, j - i);
            i = j + 1;
            size_t c = e.find(':');
            if (c == std::string::npos) continue;
            TestSet t;
            t.vsync = uint32_t(std::strtoul(e.c_str(), nullptr, 0));
            std::string rest = e.substr(c + 1);
            size_t k = 0;
            while (k < rest.size()) {
                size_t m = rest.find(',', k);
                if (m == std::string::npos) m = rest.size();
                std::string kv = rest.substr(k, m - k);
                k = m + 1;
                size_t eq = kv.find('=');
                if (eq != std::string::npos) t.kv.push_back({kv.substr(0, eq), kv.substr(eq + 1)});
            }
            g_test.sets.push_back(t);
        }
        std::sort(g_test.sets.begin(), g_test.sets.end(), [](const TestSet& a, const TestSet& b) { return a.vsync < b.vsync; });
        Log("test hook: %zu timed settings changes", g_test.sets.size());
    }
}

void Publish(bool restart = false);
/* ------------------------------------------------------------- open / close */
void SetOpen(bool open) {
    if (open == g_open) return;
    g_open = open;
    g_reset = true;
    if (open) {
        g_open_t = Now();
        g_tab_req = g_tab_req >= 0 ? g_tab_req : g_tab;
        opt_focus_first();
    } else {
        g_cap = CAP_NONE;
        xsgcfg::SaveTick(true);   // the last change goes to settings.ini now
    }
    xsginput::SetMenuOpen(open);
    Publish();   // menu_open: the game reads it (the title's idle timer)
    Log("%s", open ? "open (game input neutral)" : "closed");
}

/* ------------------------------------------------------------- settings -> game */
uint32_t FrameUs() {
    std::string v = xsgcfg::Get("game.fps_limit");
    if (v == "uncapped") return 0;
    int fps = std::atoi(v.c_str());
    if (fps <= 0) return XSG_GPU_FRAME_US_KEEP;
    return uint32_t(1000000 / fps);   // 60 -> 16666 (kernel.c maps it to its default 16666667 ns)
}

void Publish(bool restart) {
    if (!g_shared) return;
    GpuSettings* p = g_shared;
    const uint32_t s = p->seq;
    __atomic_store_n(&p->seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    p->frame_us = FrameUs();
    // audio.enabled=0 at start opens no device at all (audio.c); live, it mutes the mix
    p->audio_on = xsgcfg::GetBool("audio.enabled") ? 1 : 0;
    p->vol[0] = uint32_t(xsgcfg::GetInt("audio.master"));
    p->vol[1] = uint32_t(xsgcfg::GetInt("audio.music"));
    p->vol[2] = uint32_t(xsgcfg::GetInt("audio.effects"));
    p->vol[3] = uint32_t(xsgcfg::GetInt("audio.voice"));
    p->menu_open = g_open;

    if (restart) p->restart = 1;
    p->valid = 1;
    __atomic_store_n(&p->seq, s + 2, __ATOMIC_RELEASE);
}

int OverlayMode() {
    std::string m = xsgcfg::Get("overlay.mode");
    return m == "full" ? 2 : m == "fps" ? 1 : 0;
}

void ApplyCfg() {
    if (xsgcfg::Version() == g_seen_ver) return;
    g_seen_ver = xsgcfg::Version();
    g_overlay = OverlayMode();
    xsginput::SetDeadzone(xsgcfg::GetInt("gamepad.deadzone"), xsgcfg::GetInt("gamepad.trigger_threshold"));
    Publish();
    if (g_host.apply_video) g_host.apply_video();
}

bool RestartPending() {
    for (auto& [k, v] : g_boot_values)
        if (xsgcfg::Get(k.c_str()) != v) return true;
    return false;
}

/* ------------------------------------------------------------- style */
float g_style_scale;
ImGuiStyle g_base;

void StyleInit(ImGuiStyle& s) {
    ImGui::StyleColorsDark(&s);
    s.WindowRounding = 0;
    s.ChildRounding = 0;
    s.FrameRounding = 0;
    s.PopupRounding = 0;
    s.ScrollbarRounding = 0;
    s.GrabRounding = 0;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 0;
    s.PopupBorderSize = 1;
    s.WindowPadding = ImVec2(28, 20);
    s.FramePadding = ImVec2(12, 6);
    s.ItemSpacing = ImVec2(12, 10);
    s.ScrollbarSize = 10;
    ImVec4* c = s.Colors;
    auto C = [](ImU32 u) { return ImGui::ColorConvertU32ToFloat4(u); };
    c[ImGuiCol_Text] = C(col::kText);
    c[ImGuiCol_TextDisabled] = C(col::kTextDim);
    c[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = C(IM_COL32(16, 15, 44, 245));
    c[ImGuiCol_Border] = C(col::kFrameDim);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = C(IM_COL32(150, 180, 235, 70));
    c[ImGuiCol_ScrollbarGrabHovered] = C(IM_COL32(118, 214, 255, 140));
    c[ImGuiCol_ScrollbarGrabActive] = C(col::kCyan);
    c[ImGuiCol_NavCursor] = C(col::kCyan);
    c[ImGuiCol_Button] = C(IM_COL32(30, 32, 80, 200));
    c[ImGuiCol_ButtonHovered] = C(IM_COL32(60, 76, 150, 220));
    c[ImGuiCol_ButtonActive] = C(IM_COL32(80, 110, 190, 255));
    c[ImGuiCol_PlotLines] = C(col::kCyan);
    c[ImGuiCol_FrameBg] = C(IM_COL32(10, 10, 34, 160));
    c[ImGuiCol_ModalWindowDimBg] = C(IM_COL32(4, 4, 16, 120));
}

/* Fonts: Rajdhani (SIL OFL 1.1, fonts/OFL.txt) at 26 px per 1080 lines;
   re-rasterised when the scale changes. */
void FontsUp(float scale, float fb) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    static const ImWchar kRanges[] = {0x0020, 0x00FF, 0x2010, 0x2027, 0x2190, 0x2193, 0x25B2, 0x25CF, 0};
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    float px = std::round(27.0f * scale * fb);
    io.Fonts->AddFontFromMemoryTTF((void*)kFontBody, int(sizeof kFontBody), px, &cfg, kRanges);
    g_head = io.Fonts->AddFontFromMemoryTTF((void*)kFontHead, int(sizeof kFontHead), std::round(44.0f * scale * fb), &cfg, kRanges);
    g_small = io.Fonts->AddFontFromMemoryTTF((void*)kFontHead, int(sizeof kFontHead), std::round(20.0f * scale * fb), &cfg, kRanges);
    {   // the game UI's fonts: rasterised at the game image's text size (window height / 448 * 18)
        const float gpx = std::round(std::clamp(io.DisplaySize.y * fb / 448.0f * 20.0f, 24.0f, 96.0f));
        xsgui::g_ui_font[xsgui::kFontBody] = io.Fonts->AddFontFromMemoryTTF((void*)kGameBody, int(sizeof kGameBody), gpx, &cfg, kRanges);
        xsgui::g_ui_font[xsgui::kFontBodyBold] = io.Fonts->AddFontFromMemoryTTF((void*)kGameBold, int(sizeof kGameBold), gpx, &cfg, kRanges);
        xsgui::g_ui_font[xsgui::kFontLabel] = io.Fonts->AddFontFromMemoryTTF((void*)kGameLabel, int(sizeof kGameLabel), gpx, &cfg, kRanges);
        xsgui::g_ui_font[xsgui::kFontNumber] = io.Fonts->AddFontFromMemoryTTF((void*)kGameNumber, int(sizeof kGameNumber), gpx, &cfg, kRanges);
    }
    io.Fonts->Build();
    io.FontGlobalScale = 1.0f / fb;
    RenderFonts();
    g_font_scale = scale * fb;
}

float MenuScale(float h) { return std::max(h, 600.0f) / 1080.0f; }

/* ------------------------------------------------------------- drawing helpers */
void Backdrop(ImDrawList* dl, ImVec2 sz, float scale, float a) {
    // dim the game, then a faint grid and scanlines: the "virtual space" look
    dl->AddRectFilled(ImVec2(0, 0), sz, IM_COL32(4, 5, 22, int(120 * a)));
    dl->AddRectFilledMultiColor(ImVec2(0, 0), sz, IM_COL32(30, 30, 90, int(60 * a)), IM_COL32(30, 30, 90, int(60 * a)),
                                IM_COL32(0, 0, 10, int(90 * a)), IM_COL32(0, 0, 10, int(90 * a)));
    float step = std::round(48 * scale);
    ImU32 g = IM_COL32(110, 130, 220, int(22 * a));
    for (float x = std::fmod(sz.x * 0.5f, step); x < sz.x; x += step) dl->AddLine(ImVec2(x, 0), ImVec2(x, sz.y), g);
    for (float y = std::fmod(sz.y * 0.5f, step); y < sz.y; y += step) dl->AddLine(ImVec2(0, y), ImVec2(sz.x, y), g);
    ImU32 sl = IM_COL32(0, 0, 0, int(28 * a));
    for (float y = 0; y < sz.y; y += 4) dl->AddLine(ImVec2(0, y), ImVec2(sz.x, y), sl);
}

/* An indigo panel with a periwinkle frame, an inner grid and corner brackets. */
void Panel(ImDrawList* dl, ImVec2 a, ImVec2 b, float scale, float alpha, bool grid = true) {
    auto A = [&](ImU32 c) { return (c & 0x00ffffff) | (ImU32(((c >> 24) & 0xff) * alpha) << 24); };
    dl->AddRectFilledMultiColor(a, b, A(IM_COL32(26, 25, 66, 232)), A(IM_COL32(26, 25, 66, 232)), A(IM_COL32(14, 13, 40, 240)),
                                A(IM_COL32(14, 13, 40, 240)));
    if (grid) {
        float step = std::round(24 * scale);
        for (float x = a.x + step; x < b.x; x += step) dl->AddLine(ImVec2(x, a.y), ImVec2(x, b.y), A(col::kGrid));
        for (float y = a.y + step; y < b.y; y += step) dl->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), A(col::kGrid));
    }
    // a doubled frame: thin outer periwinkle, glossy inner edge like the minimap
    dl->AddRect(a, b, A(IM_COL32(150, 180, 235, 150)), 0, 0, 1.0f);
    dl->AddRect(ImVec2(a.x + 3 * scale, a.y + 3 * scale), ImVec2(b.x - 3 * scale, b.y - 3 * scale), A(IM_COL32(150, 180, 235, 40)));
    DrawBrackets(dl, ImVec2(a.x - 4 * scale, a.y - 4 * scale), ImVec2(b.x + 4 * scale, b.y + 4 * scale), A(col::kFrame),
                 22 * scale, 2.0f * scale);
}

/* ------------------------------------------------------------- pages */
std::string g_mc_dir;

void TabDisplay(float scale) {
    opt_begin("##display", scale);
    opt_group("WINDOW");
    opt_toggle("display.fullscreen", "Fullscreen",
               "Borderless fullscreen on the current display. F11 toggles it at any time.");
    if (xsgcfg::GetBool("display.fullscreen")) opt_disable_next("The window size applies in windowed mode.");
    opt_slider("display.window_scale", "Window size", 1, 4, 1, "%dx",
               "The window's size as a multiple of the PS2 frame (512 x 448). The window can also be resized freely.");
    static const char* kPresent[] = {"Mailbox", "FIFO (vsync)", "Immediate", "Auto"};
    opt_choice("display.present", "Present mode", kPresent,
               "Mailbox: no tearing, the game's own 59.94 Hz timer paces everything (default).\n"
               "FIFO: waits for the display's refresh. Use it on a display at 60 Hz or faster.\n"
               "Immediate: lowest latency, may tear.\nThe swapchain is recreated at once.");
    opt_group("PICTURE");
    static const char* kAspect[] = {"4:3", "16:9", "Stretch"};
    opt_choice("display.aspect", "Aspect ratio", kAspect,
               "4:3 is the original picture, letter- or pillarboxed. 16:9 stretches it to a wide frame (the game does not "
               "render more of the scene). Stretch fills the window.");
    opt_toggle("display.widescreen", "Widescreen",
               "Native 16:9 (needs the native renderer): the 3D scene is drawn wider, the PS2's 2D stays in the "
               "centre 4:3 and edge UI moves to the picture's edges. Frames the native renderer cannot draw "
               "are shown 4:3 with black bars. Overrides the aspect ratio.");
    static const char* kScale[] = {"Fit", "Integer"};
    opt_choice("display.scale_mode", "Scaling", kScale,
               "Fit: as large as the window allows. Integer: the largest whole multiple of the scanout height that fits "
               "(sharp pixels; falls back to fit below 1x).");
    static const char* kFilter[] = {"Linear", "Nearest"};
    opt_choice("display.filter", "Filter", kFilter, "How the picture is scaled to the window: smooth (linear) or sharp (nearest).");
    opt_group("GAME UI");
    static const char* kUi[] = {"Original", "Rebuilt"};
    opt_choice("display.ui", "Game UI", kUi,
               "Original: the game's own 2D, drawn for a TV at 512 x 448.\n"
               "Rebuilt: redrawn by the port, sharp at any window size: dialog, menus, shops, save screens and the "
               "battle HUD.");
    static const char* kPrompts[] = {"Auto", "Xbox", "PlayStation", "Nintendo", "Keyboard"};
    opt_choice("display.prompts", "Button prompts", kPrompts,
               "Which buttons the game's prompts show. Auto: the connected controller ([gamepad] layout), the keyboard "
               "when none is connected.");
    static const char* kPromptStyle[] = {"Default", "Light", "Retro", "Alt", "Alt 2", "Drawn"};
    opt_choice("display.prompt_style", "Prompt style", kPromptStyle,
               "The icon set's style (JulioCacko's Free Input Prompts, CC0). Drawn: the port's own simple glyphs.");
    static const char* kLayout[] = {"Game", "Wide"};
    opt_choice("display.ui_layout", "UI layout", kLayout,
               "Game: the rebuilt UI sits on the picture, as on the PS2.\n"
               "Wide: edge UI moves out to the window's edges (left panels left, right panels right); the "
               "groundwork for widescreen.");
    opt_group("CRT FILTER");
    static const char* kCrt[] = {"Off", "Scanlines", "Consumer TV", "Trinitron", "PVM", "Shadow mask"};
    const std::string crt_before = xsgcfg::Get("display.crt");
    opt_choice("display.crt", "CRT filter", kCrt,
               "The picture as on the tube television the game was made for: beams at the PS2's line structure, "
               "a phosphor mask, glow and a soft composite signal. The game's fonts and HUD were drawn for it.\n"
               "Scanlines: dark gaps only. Consumer TV: soft composite, slot mask, curved glass. Trinitron: aperture "
               "grille, sharper. PVM: a studio monitor, crisp lines, fine grille. Shadow mask: dot triads, the most glow.");
    if (xsgcfg::Get("display.crt") != crt_before)   // a new style: its own tuning
        for (const char* k : {"display.crt_scanlines", "display.crt_mask", "display.crt_halation", "display.crt_bloom",
                              "display.crt_curvature"})
            xsgcfg::Set(k, "-1");
    const bool crt_on = xsgcfg::Get("display.crt") != "off";
    if (!crt_on) opt_disable_next("Only with a CRT filter.");
    opt_slider("display.crt_strength", "CRT strength", 0, 100, 5, "%d%%", "How much of the CRT look is mixed in.");
    if (!crt_on) opt_disable_next("Only with a CRT filter.");
    opt_toggle("display.crt_interlace", "Interlace blend",
               "One beam per field line, averaging the frame's odd and even lines as an interlaced TV did: the "
               "fonts' striped fill reads solid. Off: one beam per frame line.");
    static const struct { const char* key; const char* label; const char* desc; } kTune[] = {
        {"display.crt_scanlines", "Scanlines", "How dark the gaps between beams are. -1: the style's."},
        {"display.crt_mask", "Phosphor mask", "Strength of the fine pattern of stripes or dots. -1: the style's."},
        {"display.crt_halation", "Halation", "The wide glow of light scattered in the glass. -1: the style's."},
        {"display.crt_bloom", "Bloom", "How far bright areas spread softly. -1: the style's."},
        {"display.crt_curvature", "Curvature", "The screen's bulge. -1: the style's."},
    };
    for (const auto& t : kTune) {
        if (!crt_on) opt_disable_next("Only with a CRT filter.");
        opt_slider(t.key, t.label, -1, 100, 1, "%d", t.desc);
    }
    opt_end();
}

void TabRenderer(float scale) {
    opt_begin("##renderer", scale);
    opt_group("GS BACKEND");
    static const char* kGs[] = {"GPU (paraLLEl-GS)", "Software", "Both (compare)"};
    opt_choice("renderer.gs", "GS backend", kGs,
               "GPU: the xsg-gpu process renders with paraLLEl-GS (this window and menu).\n"
               "Software: the port's software GS; no window and no menu (change it back in settings.ini).\n"
               "Both: both run and are compared (a debugging mode).");
    opt_group("RENDERER");
    static const char* kRenderer[] = {"paraLLEl-GS", "Native"};
    opt_choice("renderer.native", "Renderer", kRenderer,
               "paraLLEl-GS emulates the PS2 GS exactly at 512 x 448.\n"
               "Native redraws the 3D scenery and characters with the port's own Vulkan pipeline at a higher internal "
               "resolution and composites them over the paraLLEl-GS frame (port/docs/NATIVE_RENDERER.md).");
    if (!xsgcfg::GetBool("renderer.native")) opt_disable_next("Only with the native renderer.");
    opt_slider("renderer.native_scale", "Native internal scale", 1, 8, 1, "%dx",
               "The native renderer's resolution as a multiple of the PS2 frame. 4x is about 2048 x 1792.");
    static const char* kLighting[] = {"PS2 (per vertex)", "Per pixel"};
    if (!xsgcfg::GetBool("renderer.native")) opt_disable_next("Only with the native renderer.");
    opt_choice("renderer.lighting", "Character lighting", kLighting,
               "PS2: the game's lights evaluated per vertex, as the VU1 programs did.\n"
               "Per pixel: the same lights, colours and specular evaluated for every pixel from the interpolated "
               "normal: smoother shading on characters, the same look otherwise.");
    if (!xsgcfg::GetBool("renderer.native")) opt_disable_next("Only with the native renderer.");
    opt_slider("renderer.shadows", "Real shadows", 0, 100, 5, "%d%%",
               "Characters cast real shadows (a shadow map from the scene's key light) onto the floor, each other "
               "and themselves; the game's round blob shadows are turned off. 0: off (the game's blobs).");
    if (!xsgcfg::GetBool("renderer.native") || xsgcfg::GetInt("renderer.shadows") == 0) opt_disable_next("Only with real shadows.");
    opt_slider("renderer.shadow_angle", "Shadow light angle", 0, 85, 5, "%d\xc2\xb0",
               "The game lights characters from straight above, which would put every shadow under their feet. "
               "The shadow light leans this far from vertical, so shadows fall beside them.");
    if (!xsgcfg::GetBool("renderer.native") || xsgcfg::GetInt("renderer.shadows") == 0) opt_disable_next("Only with real shadows.");
    opt_slider("renderer.shadow_azimuth", "Shadow light direction", 0, 355, 5, "%d\xc2\xb0",
               "Which way the shadows fall (one light for every character, fixed in the world).");
    opt_group("PARALLEL-GS");
    static const char* kSsaa[] = {"Off", "2x", "4x", "8x", "16x"};
    opt_choice("renderer.ssaa", "Super-sampling", kSsaa,
               "paraLLEl-GS super-sampling: smoother edges at a GPU cost. Affects the window only; screenshots read "
               "single-sampled VRAM.");
    if (xsgcfg::GetInt("renderer.ssaa") < 2) opt_disable_next("Needs super-sampling 2x or more.");
    opt_toggle("renderer.ssaa_textures", "Super-sampled textures", "Textures are sampled at the super-sampled rate too.");
    if (xsgcfg::GetInt("renderer.ssaa") < 4) opt_disable_next("Needs super-sampling 4x or more.");
    opt_toggle("renderer.hires_scanout", "High-resolution scanout",
               "Scans out the super-sampled frame at full resolution instead of resolving it to 512 x 448.");
    opt_group("NEURAL RENDERING");
    opt_toggle("renderer.neural", "DLSS 5 Neural Rendering",
               "Re-renders every presented frame with NVIDIA's DLSS 5 neural model for a high-fidelity look "
               "(through the DLSS5VKLayer Vulkan layer and its Wine/Proton helper). Needs the layer installed and "
               "nvngx_dlssnr.dll imported (dlssnr-helper import-binaries); without them frames pass through unchanged. "
               "Style and strength are set in dlssnr-gui (or dlssnr-shmctl). Off by default; turning it on takes a restart.");
    if (RestartPending()) opt_text("Renderer changes apply after a restart: use Restart now below.", true);
    opt_end();
}

void TabGame(float scale) {
    opt_begin("##game", scale);
    opt_group("SPEED");
    static const char* kFps[] = {"60 (normal)", "30 (half speed)", "120 (double speed)", "Uncapped"};
    opt_choice("game.fps_limit", "FPS limit", kFps,
               "The game's vblank rate. The game logic runs per vblank, so this is also the game speed: 60 is normal, "
               "uncapped runs as fast as the PC allows (fast-forward). Applied live by the game process.");
    opt_group("MOVIES");
    opt_toggle("game.movies", "Play movies", "The FMV cutscenes, decoded by ffmpeg. Off skips them.");
    if (!xsgcfg::GetBool("game.movies")) opt_disable_next("Movies are off.");
    opt_edit("game.ffmpeg", "ffmpeg program", "The ffmpeg binary that decodes the movies. Empty: ffmpeg on PATH.");
    opt_group("AUDIO");
    opt_toggle("audio.enabled", "Audio", "Mutes or unmutes the game's sound output (live). Off at start opens no "
               "audio device at all.");
    opt_toggle("audio.sound_driver", "Sound driver",
               "Runs the game's IOP sound driver (RSSD). Off: no music, effects or voices are produced at all.");
    if (!xsgcfg::GetBool("audio.enabled")) opt_disable_next("Audio is muted.");
    opt_slider("audio.master", "Master volume", 0, 100, 5, "%d %%", "The whole mix, after the SPU2.");
    if (!xsgcfg::GetBool("audio.enabled")) opt_disable_next("Audio is muted.");
    opt_slider("audio.music", "Music", 0, 100, 5, "%d %%",
               "Sequenced music: the voices the sound driver plays on SPU2 core 0 (field and battle BGM).");
    if (!xsgcfg::GetBool("audio.enabled")) opt_disable_next("Audio is muted.");
    opt_slider("audio.effects", "Effects", 0, 100, 5, "%d %%", "Sound effects: SPU2 core 1 voices 0-19.");
    if (!xsgcfg::GetBool("audio.enabled")) opt_disable_next("Audio is muted.");
    opt_slider("audio.voice", "Voice and movies", 0, 100, 5, "%d %%",
               "The streamed audio: scene voice and movie soundtracks (SPU2 core 1 stream voices and AutoDMA). Scenes mix "
               "their background music into the same stream.");
    opt_group("MEMORY CARD");
    opt_toggle("game.memcard", "Memory card", "A memory card in slot 1. Off: the game sees no card.");
    if (!xsgcfg::GetBool("game.memcard")) opt_disable_next("No memory card.");
    opt_edit("game.memcard_dir", "Memory card folder",
             ("Where slot 1's saves are kept. Empty: " + g_mc_dir + ".").c_str());
    opt_group("GAME FILES");
    opt_edit("game.game_dir", "Game folder", "The folder with iso/ (the disc image) and extracted-iso/ (SLUS_204.69). "
             "Empty: the folder the port was built with.");
    opt_edit("game.iso", "Disc image", "The disc image. Empty: the first image in <game folder>/iso.");
    opt_edit("game.elf", "Main executable", "SLUS_204.69. Empty: <game folder>/extracted-iso/SLUS_204.69.");
    opt_end();
}

struct Action { const char* name; const char* label; };
const Action kActions[] = {
    {"CIRCLE", "Circle (confirm)"}, {"CROSS", "Cross (cancel)"}, {"TRIANGLE", "Triangle"}, {"SQUARE", "Square"},
    {"START", "Start"}, {"SELECT", "Select"}, {"UP", "D-pad up"}, {"DOWN", "D-pad down"}, {"LEFT", "D-pad left"},
    {"RIGHT", "D-pad right"}, {"L1", "L1"}, {"R1", "R1"}, {"L2", "L2"}, {"R2", "R2"}, {"L3", "L3"}, {"R3", "R3"},
    {"LS_UP", "Left stick up"}, {"LS_DOWN", "Left stick down"}, {"LS_LEFT", "Left stick left"},
    {"LS_RIGHT", "Left stick right"}, {"RS_UP", "Right stick up"}, {"RS_DOWN", "Right stick down"},
    {"RS_LEFT", "Right stick left"}, {"RS_RIGHT", "Right stick right"},
};

#ifdef XSG_GPU_SDL
std::string PadLabel(int b) {
    std::string layout = xsgcfg::Get("gamepad.layout");
    static const char* kXbox[] = {"A", "B", "X", "Y"};
    static const char* kPs[] = {"Cross", "Circle", "Square", "Triangle"};
    static const char* kNin[] = {"B", "A", "Y", "X"};
    if (b >= 0 && b < 4) return layout == "playstation" ? kPs[b] : layout == "nintendo" ? kNin[b] : kXbox[b];
    switch (b) {
    case SDL_GAMEPAD_BUTTON_BACK: return layout == "playstation" ? "Share" : layout == "nintendo" ? "Minus" : "Back";
    case SDL_GAMEPAD_BUTTON_START: return layout == "playstation" ? "Options" : layout == "nintendo" ? "Plus" : "Start";
    case SDL_GAMEPAD_BUTTON_GUIDE: return "Guide";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "L-stick press";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "R-stick press";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return layout == "playstation" ? "L1" : layout == "nintendo" ? "L" : "LB";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return layout == "playstation" ? "R1" : layout == "nintendo" ? "R" : "RB";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-pad up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad right";
    case SDL_GAMEPAD_BUTTON_COUNT: return layout == "playstation" ? "L2" : layout == "nintendo" ? "ZL" : "LT";
    case SDL_GAMEPAD_BUTTON_COUNT + 1: return layout == "playstation" ? "R2" : layout == "nintendo" ? "ZR" : "RT";
    default: return xsginput::PadButtonName(b);
    }
}

void BindRows(float scale) {
    (void)scale;
    auto keys = xsginput::KeyBinds();
    auto pads = xsginput::PadBinds();
    opt_bind_header("KEY 1", "KEY 2", "GAMEPAD");
    for (const Action& a : kActions) {
        uint32_t m = xsginput::TargetMask(a.name);
        std::vector<int> ks;
        for (auto& [sc, mm] : keys)
            if (mm == m) ks.push_back(sc);
        int pb = -1;
        for (auto& [b, mm] : pads)
            if (mm == m) { pb = b; break; }
        std::string c0 = ks.size() > 0 ? SDL_GetScancodeName(SDL_Scancode(ks[0])) : "";
        std::string c1 = ks.size() > 1 ? SDL_GetScancodeName(SDL_Scancode(ks[1])) : "";
        std::string c2 = pb >= 0 ? PadLabel(pb) : "";
        const char* cells[3] = {c0.c_str(), c1.c_str(), c2.c_str()};
        const bool en[3] = {true, true, true};
        std::string desc = std::string("DualShock 2 ") + a.label +
                           ".\n\nSelect a cell and press Enter / A, then the new key or button (Esc cancels). Delete, "
                           "Backspace or the gamepad's left face button clears a cell.";
        int r = opt_bind_row(a.name, a.label, desc.c_str(), cells, en);
        if (r < 0) continue;
        int cell = r % 10;
        int old = cell == 0 ? (ks.size() > 0 ? ks[0] : -1) : cell == 1 ? (ks.size() > 1 ? ks[1] : -1) : pb;
        if (r >= 10) {   // clear
            if (old >= 0) {
                if (cell < 2) xsginput::SetKeyBind(old, 0);
                else xsginput::SetPadBind(old, 0);
                xsginput::SaveBinds();
            }
        } else {
            g_cap = cell < 2 ? CAP_KEY : CAP_PAD;
            g_cap_mask = m;
            g_cap_old = old;
            g_cap_t = Now();
            int lx, ly;
            xsginput::MenuPad(&g_cap_pad_prev, &lx, &ly);
        }
    }
}

void FinishCapture(int code) {
    if (g_cap == CAP_KEY) {
        if (g_cap_old >= 0) xsginput::SetKeyBind(g_cap_old, 0);
        xsginput::SetKeyBind(code, g_cap_mask);
        Log("bind: key %s -> %s", SDL_GetScancodeName(SDL_Scancode(code)), xsginput::TargetNames(g_cap_mask).c_str());
    } else {
        for (auto& [b, mm] : xsginput::PadBinds())
            if (mm == g_cap_mask) xsginput::SetPadBind(b, 0);
        xsginput::SetPadBind(code, g_cap_mask);
        Log("bind: gamepad %s -> %s", xsginput::PadButtonName(code), xsginput::TargetNames(g_cap_mask).c_str());
    }
    xsginput::SaveBinds();
    g_cap = CAP_NONE;
}
#endif

void TabControls(float scale) {
    opt_begin("##controls", scale);
    opt_group("GAMEPAD");
    static const char* kLayout[] = {"Xbox", "PlayStation", "Nintendo"};
    opt_choice("gamepad.layout", "Button labels", kLayout,
               "How the gamepad's buttons are named here. Nintendo also swaps confirm and back in this menu (A on the "
               "right). Game bindings are by position, so the game is unaffected.");
    opt_slider("gamepad.deadzone", "Stick dead zone", 0, 32000, 500, "%d",
               "Stick travel (of 32767) ignored around the centre. Default 8000.");
    opt_slider("gamepad.trigger_threshold", "Trigger threshold", 0, 32000, 500, "%d",
               "Trigger travel that counts as an L2 / R2 press. Default 8000.");
    opt_group("BINDINGS");
#ifdef XSG_GPU_SDL
    BindRows(scale);
    if (opt_button("##reset_binds", "Reset all bindings", "Back to the default keyboard and gamepad layout "
                   "(port/docs/STAGE10_INPUT.md section 2)."))
        xsginput::ResetBinds();
#else
    opt_text("Rebinding needs SDL3.");
#endif
    opt_end();
}

void TabInfo(float scale) {
    opt_begin("##info", scale);
    opt_group("OVERLAY");
    static const char* kOverlay[] = {"Off", "FPS", "Full"};
    opt_choice("overlay.mode", "Performance overlay", kOverlay,
               "F10 cycles it: off, the frame rate, or a full panel with 1% / 0.1% lows, a frame-time graph, resolutions, "
               "present mode, GPU and the renderer's counters.");
    std::string sv = g_shot_note.empty() || Now() - g_shot_note_t > 4 ? "" : g_shot_note;
    if (opt_button("##shot", "Take screenshot", "Writes a PNG of the current game frame (without the menu) to "
                   "[game] screenshot_dir, default ~/Pictures/xsg (Debug tab).", sv.empty() ? "PNG" : sv.c_str()) &&
        g_host.screenshot) {
        std::string dir;
        if (const char* d = xsgcfg::CStr("game.screenshot_dir")) dir = d;
        else if (const char* h = std::getenv("HOME"); h && *h) dir = std::string(h) + "/Pictures/xsg";
        else dir = ".";
        char name[64];
        std::time_t t = std::time(nullptr);
        std::strftime(name, sizeof name, "/xsg_%Y%m%d_%H%M%S.png", std::localtime(&t));
        g_host.screenshot(dir + name);
        g_shot_note = "SAVED";
        g_shot_note_t = Now();
    }
    opt_group("BUILD");
    opt_info("##ver", "Version", XSG_BUILD_VERSION, "The port's git revision at build time.");
    static std::string imv = std::string("Dear ImGui ") + IMGUI_VERSION;
    opt_info("##imgui", "Menu", imv.c_str(), "The menu is drawn with Dear ImGui (MIT) through Granite.");
    static std::string gpu = g_dev ? g_dev->get_gpu_properties().deviceName : "";
    opt_info("##gpu", "GPU", gpu.c_str(), "The Vulkan device xsg-gpu renders on.");
    std::string path = xsgcfg::Path();
    opt_info("##ini", "Settings file", path.c_str(), "Every setting of the port is kept here and every one can be changed "
             "in this menu. XSG_CONFIG=<file> picks another file; no other environment variable sets anything.");
    if (!xsgcfg::IgnoredEnv().empty()) {
        opt_group("IGNORED XSG_* ENVIRONMENT");
        opt_text("These are no longer read: set them in settings.ini (or here) instead.", true);
        for (auto& l : xsgcfg::IgnoredEnv()) opt_text(l.c_str());
    }
    opt_end();
}

/* Every [debug] key of the table (port/runtime/xsg_settings_table.h), in its
   order, each with the row its value list calls for. */
void TabDebug(float scale) {
    opt_begin("##debug", scale);
    opt_group("RUNTIME");
    static const char* kOverlay[] = {"Off", "FPS", "Full"};
    opt_choice("overlay.mode", "Performance overlay", kOverlay, "F10 cycles it.");
    opt_edit("game.screenshot_dir", "Screenshot folder", "Where Take screenshot (Info) writes. Empty: ~/Pictures/xsg.");
    opt_group("DEVELOPER SWITCHES (AFTER RESTART)");
    opt_text("Traces, captures, logs and test hooks. They are read when the game starts.");
    for (int i = 0; const xsgcfg::Meta* m = xsgcfg::At(i); ++i) {
        if (std::strncmp(m->key, "debug.", 6)) continue;
        opt_auto(m->key, m->key + 6, m->desc);
    }
    if (RestartPending()) opt_text("Changes apply after a restart: use Restart now below.", true);
    opt_end();
}

/* ------------------------------------------------------------- overlay */
void Percentiles(float* avg, float* low1, float* low01, float* maxms) {
    // lows over the last ~10 s of presents (TotK Recomp's perfmon: the frame rate of the slowest 1% / 0.1% frames)
    std::vector<float> v(g_ft.begin(), g_ft.end());
    *avg = *low1 = *low01 = *maxms = 0;
    if (v.empty()) return;
    double sum = 0;
    for (float f : v) sum += f;
    *avg = float(sum / v.size());
    std::sort(v.begin(), v.end(), std::greater<float>());
    auto low = [&](double frac) {
        size_t n = std::max<size_t>(1, size_t(v.size() * frac));
        double s = 0;
        for (size_t i = 0; i < n; ++i) s += v[i];
        return float(1000.0 / (s / n));
    };
    *low1 = low(0.01);
    *low01 = low(0.001);
    *maxms = v[0];
}

void OverlayDraw(float scale) {
    if (!g_overlay) return;
    ImVec2 sz = ImGui::GetIO().DisplaySize;
    ImDrawList* dl = g_open ? ImGui::GetBackgroundDrawList() : ImGui::GetForegroundDrawList();
    if (g_overlay == 1) {
        char t[48];
        std::snprintf(t, sizeof t, "%.0f FPS", g_fps_shown);
        ImVec2 ts = ImGui::CalcTextSize(t);
        float pad = 7 * scale;
        ImVec2 a(sz.x - 14 * scale - ts.x - pad * 2, 14 * scale), b(sz.x - 14 * scale, 14 * scale + ts.y + pad * 2);
        dl->AddRectFilled(a, b, IM_COL32(14, 13, 44, 190));
        dl->AddRect(a, b, IM_COL32(150, 180, 235, 120));
        DrawBrackets(dl, a, b, col::kCyan, 6 * scale, 1.5f * scale);
        dl->AddText(ImVec2(a.x + pad, a.y + pad), col::kText, t);
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(sz.x - 14 * scale, 14 * scale), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(420 * scale, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14 * scale, 10 * scale));
    ImGui::Begin("##perf", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                                        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleVar();
    if (g_open) ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindow());
    else ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    Panel(ImGui::GetWindowDrawList(), w->Pos, ImVec2(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y), scale * 0.6f, 0.9f, false);
    ImGui::PushFont(g_small);
    float avg, l1, l01, mx;
    Percentiles(&avg, &l1, &l01, &mx);
    const ImVec4 lab = ImGui::ColorConvertU32ToFloat4(col::kSteel), cy = ImGui::ColorConvertU32ToFloat4(col::kCyan);
    ImGui::TextColored(cy, "XSG-GPU  PERFORMANCE");
    auto row = [&](const char* l, const char* fmt, ...) {
        char b[128];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(b, sizeof b, fmt, ap);
        va_end(ap);
        ImGui::TextColored(lab, "%s", l);
        ImGui::SameLine(150 * scale);
        ImGui::TextUnformatted(b);
    };
    row("FPS", "%.1f    1%% low %.1f    0.1%% low %.1f", g_fps_shown, l1, l01);
    row("FRAME", "%.2f ms avg    %.1f ms max", avg, mx);
    // frame-time graph, last 240 presents
    {
        float vals[240];
        int n = int(std::min<size_t>(240, g_ft.size()));
        for (int i = 0; i < n; ++i) vals[i] = g_ft[(g_ft_pos + g_ft.size() - n + i) % g_ft.size()];
        float top = std::max(34.0f, mx);
        ImGui::PlotLines("##ft", vals, n, 0, nullptr, 0, top, ImVec2(-FLT_MIN, 54 * scale));
    }
    row("WINDOW", "%u x %u", g_st.target_w, g_st.target_h);
    row("IMAGE", "%u x %u   (scanout %u x %u)", g_st.image_w, g_st.image_h, g_st.scan_w, g_st.scan_h);
    row("PRESENT", "%s", g_st.present);
    static std::string gpu;
    static double vram_gb;
    if (gpu.empty() && g_dev) {
        gpu = g_dev->get_gpu_properties().deviceName;
        VkPhysicalDeviceMemoryProperties mp;
        vkGetPhysicalDeviceMemoryProperties(g_dev->get_physical_device(), &mp);
        uint64_t v = 0;
        for (uint32_t h = 0; h < mp.memoryHeapCount; h++)
            if (mp.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) v = std::max<uint64_t>(v, mp.memoryHeaps[h].size);
        vram_gb = v / double(1u << 30);
    }
    row("GPU", "%s", gpu.c_str());
    row("VRAM", "%.1f GB", vram_gb);
    row("RING", "%.2f MB / %u MB (%.1f%%)", g_st.ring_used / 1048576.0, g_st.ring_size >> 20,
        g_st.ring_size ? 100.0 * g_st.ring_used / g_st.ring_size : 0.0);
    row("VSYNCS", "%llu   presents %llu", (unsigned long long)g_st.vsyncs, (unsigned long long)g_st.presents);
    if (g_st.native_on)
        row("NATIVE", "%u batches/frame   frames native %llu / VU1+GS %llu", g_st.native_batches,
            (unsigned long long)g_st.native_presents, (unsigned long long)(g_st.presents - g_st.native_presents));
    else
        row("NATIVE", "off (all 3D through VU1 + paraLLEl-GS)");
    ImGui::PopFont();
    ImGui::End();
}

/* ------------------------------------------------------------- the menu window */
ImGuiID g_footer_first;   // CONTINUE, where Down from the last option row lands
ImGuiID g_footer[3];      // CONTINUE, RESTART NOW, QUIT GAME
ImGuiWindow* g_footer_row_win;

/* Keyboard / gamepad focus for the footer buttons (CONTINUE, RESTART NOW, QUIT
   GAME).  Down past the last option row has nothing below it inside the rows'
   scroll area, so ImGui's move request ends without a result: it lands on
   CONTINUE here.  On the footer, Left/Right step between the buttons and Up goes
   back to the row it came from (handled here: ImGui's own move requests find
   nothing from a programmatic focus outside the rows' scope). */
ImGuiID g_footer_row;   // the option row Down left from

void FooterNav() {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* menu = ImGui::FindWindowByName("##menu");
    if (!g_footer[0] || !menu) return;
    int on = -1;
    for (int i = 0; i < 3; ++i)
        if (g_footer[i] && g.NavId == g_footer[i]) on = i;
    auto pressed = [](ImGuiKey a, ImGuiKey b) { return ImGui::IsKeyPressed(a, true) || ImGui::IsKeyPressed(b, true); };
    auto focus = [&](ImGuiID id, ImGuiWindow* w) {
        ImGui::NavMoveRequestCancel();
        ImGui::SetFocusID(id, w);
        ImGui::SetNavCursorVisible(true);
    };
    if (on >= 0) {
        if (pressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft)) focus(g_footer[(on + 2) % 3], menu);
        else if (pressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight)) focus(g_footer[(on + 1) % 3], menu);
        else if (pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp)) {
            if (g_footer_row) {
                ImGui::NavMoveRequestCancel();
                ImGui::SetFocusID(g_footer_row, g_footer_row_win);
                ImGui::SetNavCursorVisible(true);
            }
        } else if (g.NavMoveDir != ImGuiDir_None) ImGui::NavMoveRequestCancel();   // Down: the bottom row, no wrap
        return;
    }
    if (g.NavMoveDir != ImGuiDir_Down || !g.NavMoveScoringItems || !g.NavWindow) return;
    if (g.NavMoveResultLocal.ID || g.NavMoveResultLocalVisible.ID || g.NavMoveResultOther.ID) return;
    g_footer_row = g.NavId;
    g_footer_row_win = g.NavWindow;
    focus(g_footer[0], menu);
}

void Bottom(float scale, float w) {
    float bh = std::round(44 * scale), bw = std::round(190 * scale);
    auto button = [&](const char* id, const char* label, bool accent) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool click = ImGui::InvisibleButton(id, ImVec2(bw, bh));
        bool sel = ImGui::IsItemHovered() || (ImGui::IsItemFocused() && GImGui->NavCursorVisible);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 b(p.x + bw, p.y + bh);
        dl->AddRectFilled(p, b, sel ? IM_COL32(60, 76, 150, 200) : IM_COL32(14, 14, 44, 200));
        dl->AddRect(p, b, sel ? col::kCyan : accent ? col::kWarn : col::kFrameDim);
        if (sel) DrawBrackets(dl, ImVec2(p.x - 3, p.y - 3), ImVec2(b.x + 3, b.y + 3), col::kCyan, 7 * scale, 1.5f * scale);
        ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(p.x + (bw - ts.x) * 0.5f, p.y + (bh - ts.y) * 0.5f), accent && !sel ? col::kWarn : col::kText, label);
        return click;
    };
    if (button("##continue", "CONTINUE", false)) SetOpen(false);
    g_footer_first = g_footer[0] = ImGui::GetItemID();
    ImGui::SameLine();
    bool pend = RestartPending();
    const bool restart_click = button("##restart", pend ? "RESTART NOW  *" : "RESTART NOW", pend);
    g_footer[1] = ImGui::GetItemID();
    if (restart_click) {
        xsgcfg::SaveNow();
        Log("restart: asking the game to restart");
        Publish(true);
    }
    ImGui::SameLine();
    const bool quit_click = button("##quit", "QUIT GAME", false);
    g_footer[2] = ImGui::GetItemID();
    if (quit_click && g_host.quit) {
        xsgcfg::SaveNow();
        g_host.quit();
    }
    ImGui::SameLine();
    const char* hint = g_cap ? "Press the new key or button.   Esc / wait: cancel"
                             : "LB / RB  Q / E  tabs      A / Enter  change      B / Esc  close";
    ImGui::PushFont(g_small);
    ImVec2 ts = ImGui::CalcTextSize(hint);
    ImVec2 c = ImGui::GetCursorScreenPos();
    float right = ImGui::GetWindowPos().x + w - ImGui::GetStyle().WindowPadding.x;
    ImGui::GetWindowDrawList()->AddText(ImVec2(std::max(c.x + 12 * scale, right - ts.x), c.y + (bh - ts.y) * 0.5f),
                                        col::kTextDim, hint);
    ImGui::PopFont();
}

void TabBar(float scale, float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float x = p.x, h = std::round(40 * scale);
    for (int i = 0; i < TAB_COUNT; ++i) {
        ImVec2 ts = ImGui::CalcTextSize(kTabNames[i]);
        float tw = ts.x + 40 * scale;
        ImGui::SetCursorScreenPos(ImVec2(x, p.y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##tab", ImVec2(tw, h), ImGuiButtonFlags_None) && g_tab != i) {
            g_tab_req = i;
        }
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        bool cur = g_tab == i;
        ImVec2 a(x, p.y), b(x + tw, p.y + h);
        if (cur) {
            dl->AddRectFilledMultiColor(a, b, IM_COL32(118, 214, 255, 10), IM_COL32(118, 214, 255, 10),
                                        IM_COL32(118, 214, 255, 60), IM_COL32(118, 214, 255, 60));
            dl->AddLine(ImVec2(a.x, b.y - 1), ImVec2(b.x, b.y - 1), col::kCyan, 2.5f * scale);
            DrawBrackets(dl, a, b, col::kCyan, 6 * scale, 1.2f * scale);
        } else if (hov) {
            dl->AddRectFilled(a, b, IM_COL32(150, 180, 235, 20));
        }
        dl->AddText(ImVec2(x + 20 * scale, p.y + (h - ts.y) * 0.5f), cur ? col::kText : hov ? col::kText : col::kTextDim,
                    kTabNames[i]);
        x += tw + 6 * scale;
    }
    dl->AddLine(ImVec2(p.x, p.y + h), ImVec2(p.x + w, p.y + h), col::kFrameDim);
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 14 * scale));
}

void LayoutMenu(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 sz = io.DisplaySize;
    float a = g_fade;
    Backdrop(ImGui::GetBackgroundDrawList(), sz, scale, a);
    float w = std::min(sz.x - 40 * scale, std::round(1500 * scale)), h = std::min(sz.y - 40 * scale, std::round(900 * scale));
    float slide = (1 - a) * 16 * scale;   // a quick fade and rise on opening
    ImGui::SetNextWindowPos(ImVec2(std::round((sz.x - w) * 0.5f), std::round((sz.y - h) * 0.5f + slide)));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a);
    ImGui::Begin("##menu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Panel(dl, win->Pos, ImVec2(win->Pos.x + w, win->Pos.y + h), scale, a);
    // header: title, a thin rule and a status tag
    ImGui::PushFont(g_head);
    ImVec2 tp = ImGui::GetCursorScreenPos();
    dl->AddText(tp, col::kText, "XENOSAGA EPISODE I \xe2\x80\x94 PC");
    float th = ImGui::GetFontSize();
    ImGui::PopFont();
    ImGui::PushFont(g_small);
    const char* tag = "SYSTEM CONFIGURATION";
    ImVec2 tg = ImGui::CalcTextSize(tag);
    float rx = win->Pos.x + w - ImGui::GetStyle().WindowPadding.x;
    dl->AddText(ImVec2(rx - tg.x, tp.y + th - tg.y - 4 * scale), col::kSteel, tag);
    ImGui::PopFont();
    dl->AddLine(ImVec2(tp.x, tp.y + th + 6 * scale), ImVec2(rx, tp.y + th + 6 * scale), col::kFrameDim);
    dl->AddRectFilled(ImVec2(tp.x, tp.y + th + 5 * scale), ImVec2(tp.x + 120 * scale, tp.y + th + 8 * scale), col::kCyan);
    ImGui::SetCursorScreenPos(ImVec2(tp.x, tp.y + th + 18 * scale));
    float cw = w - ImGui::GetStyle().WindowPadding.x * 2;
    TabBar(scale, cw);
    opt_reserve_bottom(std::round(44 * scale) + 18 * scale);
    switch (g_tab) {
    case TAB_DISPLAY: TabDisplay(scale); break;
    case TAB_RENDERER: TabRenderer(scale); break;
    case TAB_GAME: TabGame(scale); break;
    case TAB_CONTROLS: TabControls(scale); break;
    case TAB_INFO: TabInfo(scale); break;
    default: TabDebug(scale); break;
    }
    ImGui::SetCursorScreenPos(ImVec2(tp.x, win->Pos.y + h - ImGui::GetStyle().WindowPadding.y - std::round(44 * scale)));
    Bottom(scale, w);
    FooterNav();
    // the capture prompt
    if (g_cap) {
        ImVec2 c(win->Pos.x + w * 0.5f, win->Pos.y + h * 0.5f);
        ImVec2 pa(c.x - 260 * scale, c.y - 60 * scale), pb(c.x + 260 * scale, c.y + 60 * scale);
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        fg->AddRectFilled(ImVec2(0, 0), sz, IM_COL32(0, 0, 10, 110));
        Panel(fg, pa, pb, scale, 1.0f);
        const char* t1 = g_cap == CAP_KEY ? "PRESS A KEY" : "PRESS A GAMEPAD BUTTON";
        std::string t2 = "for " + xsginput::TargetNames(g_cap_mask) + "   (Esc cancels)";
        ImVec2 s1 = ImGui::CalcTextSize(t1), s2 = ImGui::CalcTextSize(t2.c_str());
        fg->AddText(ImVec2(c.x - s1.x * 0.5f, c.y - s1.y - 4 * scale), col::kCyan, t1);
        fg->AddText(ImVec2(c.x - s2.x * 0.5f, c.y + 4 * scale), col::kTextDim, t2.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // tab switching and closing (not while a binding waits for its press or a text field is open)
    if (!g_cap && !opt_editing()) {
        bool nin = xsgcfg::Get("gamepad.layout") == "nintendo";
        (void)nin;
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) || ImGui::IsKeyPressed(ImGuiKey_Q, false))
            g_tab_req = (g_tab + TAB_COUNT - 1) % TAB_COUNT;
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false) || ImGui::IsKeyPressed(ImGuiKey_E, false))
            g_tab_req = (g_tab + 1) % TAB_COUNT;
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            SetOpen(false);
    }
}

}  // namespace

/* ======================================================================== API */
#ifdef XSG_GPU_SDL
void Init(Vulkan::Device* dev, SDL_Window* win, GpuSettings* shared, const Host& host) {
    g_win = win;
#else
void Init(Vulkan::Device* dev, void*, GpuSettings* shared, const Host& host) {
#endif
    g_dev = dev;
    g_shared = shared;
    g_host = host;
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigNavEscapeClearFocusItem = false;
    StyleInit(g_base);
    ImGui::GetStyle() = g_base;
#ifdef XSG_GPU_SDL
    if (win) {
        ImGui_ImplSDL3_InitForVulkan(win);
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, nullptr, 0);   // fed from xsg_input (menu.cpp)
    }
#endif
    io.BackendPlatformName = io.BackendPlatformName ? io.BackendPlatformName : "xsg_menu";
    io.BackendRendererName = "xsg_menu_granite";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    g_ok = RenderInit(dev);
    if (!g_ok) Log("renderer set-up failed; the menu stays closed");
    TestParse();
    // read-only facts for the pages
    if (const char* h = std::getenv("HOME"); h && *h) g_mc_dir = std::string(h) + "/.local/share/xsg/mc0";   // the default
    for (int i = 0; const xsgcfg::Meta* m = xsgcfg::At(i); ++i)
        if (!m->live) g_boot_values.push_back({m->key, xsgcfg::Get(m->key)});
    g_overlay = OverlayMode();
    g_seen_ver = xsgcfg::Version();
    xsginput::SetDeadzone(xsgcfg::GetInt("gamepad.deadzone"), xsgcfg::GetInt("gamepad.trigger_threshold"));
    Publish();
    Log("ready: F1 / Esc or Back+Start opens, F10 overlay (%s); settings %s", xsgcfg::Get("overlay.mode").c_str(),
        xsgcfg::Path().c_str());
}

bool IsOpen() { return g_open; }

#ifdef XSG_GPU_SDL
bool Event(const SDL_Event& ev) {
    if (!g_ok) return false;
    SDL_Event e = ev;
    // synthetic key events (XSG_GPU_INPUT_TEST) carry no window; the SDL3 backend drops those
    if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && !e.key.windowID && g_win)
        e.key.windowID = SDL_GetWindowID(g_win);
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN: {
        const SDL_Scancode sc = e.key.scancode;
        if (g_cap == CAP_KEY && !e.key.repeat) {
            if (sc == SDL_SCANCODE_ESCAPE) g_cap = CAP_NONE;
            else if (sc != SDL_SCANCODE_F1 && sc != SDL_SCANCODE_F10 && sc != SDL_SCANCODE_F11) FinishCapture(sc);
            return true;
        }
        if (g_cap == CAP_PAD && sc == SDL_SCANCODE_ESCAPE) { g_cap = CAP_NONE; return true; }
        if (!e.key.repeat && sc == SDL_SCANCODE_F1) { SetOpen(!g_open); return true; }
        if (!e.key.repeat && sc == SDL_SCANCODE_ESCAPE && !g_open) {
            bool bound = false;
            for (auto& [k, m] : xsginput::KeyBinds()) bound |= k == SDL_SCANCODE_ESCAPE;
            if (!bound) { SetOpen(true); return true; }
            return false;
        }
        if (sc == SDL_SCANCODE_F10) {
            if (!e.key.repeat) {
                static const char* kModes[] = {"off", "fps", "full"};
                xsgcfg::Set("overlay.mode", kModes[(OverlayMode() + 1) % 3]);
            }
            return true;
        }
        if (!g_open) return false;
        if (g_win) ImGui_ImplSDL3_ProcessEvent(&e);
        return true;
    }
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        if (!g_open) return false;
        if (g_win) ImGui_ImplSDL3_ProcessEvent(&e);
        return true;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        if (g_win) ImGui_ImplSDL3_ProcessEvent(&e);
        return false;
    default: return false;
    }
}
#endif

static bool g_game_menu_up;   // a rebuilt game menu / the pause screen is showing
static void PadFrame() {
#ifdef XSG_GPU_SDL
    uint32_t b = 0;
    int lx = 0, ly = 0;
    bool have = xsginput::MenuPad(&b, &lx, &ly);
    xsgui::PromptsFrame(have);
    ImGuiIO& io = ImGui::GetIO();
    if (have) io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    else io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    // the open combo: Back + Start (or Guide), on the press
    bool combo = ((b >> SDL_GAMEPAD_BUTTON_BACK) & 1) && ((b >> SDL_GAMEPAD_BUTTON_START) & 1);
    combo = combo || ((b >> SDL_GAMEPAD_BUTTON_GUIDE) & 1);
    // with a game menu or the pause screen up (rebuilt UI), View / Back alone opens the settings
    // (the menus show it as a "Settings" hint)
    static bool back_prev;
    const bool back = (b >> SDL_GAMEPAD_BUTTON_BACK) & 1;
    if (back && !back_prev && !((b >> SDL_GAMEPAD_BUTTON_START) & 1) && g_game_menu_up && !g_open && !g_cap) SetOpen(true);
    back_prev = back;
    if (combo && !g_combo_prev && !g_cap) SetOpen(!g_open);
    g_combo_prev = combo;
    if (g_cap == CAP_PAD) {
        uint32_t nb = b & ~g_cap_pad_prev;
        g_cap_pad_prev = b;
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT && i < 32; ++i)
            if ((nb >> i) & 1) { FinishCapture(i); g_pad_prev = b; return; }
        // triggers: read through the binds' pseudo buttons via the pad state is not exposed; use the axes of SDL
        int n = 0;
        if (SDL_JoystickID* ids = SDL_GetGamepads(&n)) {
            for (int i = 0; i < n; ++i)
                if (SDL_Gamepad* gp = SDL_GetGamepadFromID(ids[i])) {
                    if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16000) { FinishCapture(SDL_GAMEPAD_BUTTON_COUNT); break; }
                    if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000) { FinishCapture(SDL_GAMEPAD_BUTTON_COUNT + 1); break; }
                }
            SDL_free(ids);
        }
        if (Now() - g_cap_t > 8) g_cap = CAP_NONE;   // nothing pressed: give up
        g_pad_prev = b;
        return;
    }
    if (g_cap == CAP_KEY && Now() - g_cap_t > 8) g_cap = CAP_NONE;
    if (!g_open) { g_pad_prev = b; g_stick_dir = -1; return; }
    g_stick_dir = StickToDpad(lx, ly);
    uint32_t eff = b;
    if (g_stick_dir == ImGuiKey_GamepadDpadUp) eff |= 1u << SDL_GAMEPAD_BUTTON_DPAD_UP;
    if (g_stick_dir == ImGuiKey_GamepadDpadDown) eff |= 1u << SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    if (g_stick_dir == ImGuiKey_GamepadDpadLeft) eff |= 1u << SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    if (g_stick_dir == ImGuiKey_GamepadDpadRight) eff |= 1u << SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    if (g_reset) { g_pad_prev = eff; return; }   // a button held when the menu opened counts once released
    const bool nin = xsgcfg::Get("gamepad.layout") == "nintendo";
    const struct { int bit; ImGuiKey key; } map[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, nin ? ImGuiKey_GamepadFaceRight : ImGuiKey_GamepadFaceDown},
        {SDL_GAMEPAD_BUTTON_EAST, nin ? ImGuiKey_GamepadFaceDown : ImGuiKey_GamepadFaceRight},
        {SDL_GAMEPAD_BUTTON_WEST, ImGuiKey_GamepadFaceLeft}, {SDL_GAMEPAD_BUTTON_NORTH, ImGuiKey_GamepadFaceUp},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, ImGuiKey_GamepadL1}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, ImGuiKey_GamepadR1},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, ImGuiKey_GamepadDpadUp}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN, ImGuiKey_GamepadDpadDown},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, ImGuiKey_GamepadDpadLeft}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, ImGuiKey_GamepadDpadRight},
    };
    for (auto& k : map) {
        uint32_t m = 1u << k.bit;
        if ((eff ^ g_pad_prev) & m) io.AddKeyEvent(k.key, (eff & m) != 0);
    }
    g_pad_prev = eff;
#endif
}

void Frame(uint32_t vsync, const Stats& s) {
    if (!g_ok) return;
    g_st = s;
    PerfTick();
    // test hook: timed settings and the per-tab pages
    while (g_test.next_set < g_test.sets.size() && vsync >= g_test.sets[g_test.next_set].vsync) {
        for (auto& [k, v] : g_test.sets[g_test.next_set].kv) {
            if (k == "SAVE") xsgcfg::SaveNow();
            else if (k == "OPEN") SetOpen(v != "0");
            else if (k == "TAB") g_tab_req = std::atoi(v.c_str());
            else if (k == "SHOT" && g_host.screenshot) g_host.screenshot(v);
            else if (k == "RESTART") { xsgcfg::SaveNow(); Publish(true); }
            else { xsgcfg::Set(k.c_str(), v); Log("test: %s = %s at vsync %u", k.c_str(), v.c_str(), vsync); }
        }
        g_test.next_set++;
    }
    g_test.shot.clear();
    if (g_test.at && vsync >= g_test.at && g_test.page < 0) {
        g_test.at = 0;
        g_test.page = 0;
        g_test.page_t = vsync;
        g_test.saved_overlay = g_overlay;
        g_tab_req = 0;
        SetOpen(true);
    }
    if (g_test.page >= 0 && vsync >= g_test.page_t + g_test.per) {
        static const char* kNames[] = {"display", "renderer", "game", "controls", "info", "debug", "overlay"};
        g_test.shot = g_test.dir + "/menu_" + std::to_string(g_test.page) + "_" + kNames[g_test.page] + ".png";
        g_test.page_t = vsync;
        // the shot is of this frame; the next page starts at the next one (handled after the layout below)
    }
    ApplyCfg();
    if (g_test.page == TAB_COUNT) g_overlay = 2;   // the overlay page: menu closed, full panel
    xsgcfg::SaveTick(false);
    if (g_open) g_fade = float(std::min(1.0, (Now() - g_open_t) / 0.16));
    g_visible = false;
    const bool rebuilt = xsgcfg::Get("display.ui") == "rebuilt" && s.game_w > 0;
    const bool dialog = rebuilt && xsgui::DialogActive(vsync);
    const bool battle = rebuilt && xsgui::BattleActive(vsync);
    const bool menus = rebuilt && xsgui::MenuActive(vsync);
    g_game_menu_up = menus && xsgui::MenuWantsSettings();
    if (g_shared) {   // the game asked for the settings menu (the title's Settings choice)
        static uint32_t seen = ~0u;
        const uint32_t r = __atomic_load_n(&g_shared->open_req, __ATOMIC_ACQUIRE);
        if (seen == ~0u) seen = r;
        else if (r != seen) { seen = r; if (!g_open) SetOpen(true); }
    }
    if (!g_open && !g_overlay && !dialog && !battle && !menus) { PadFrame(); return; }

    ImGuiIO& io = ImGui::GetIO();
    float fb = 1.0f;
    uint32_t tw = s.target_w, th = s.target_h;
    if (g_test.page >= 0 || !s.windowed) {
        io.DisplaySize = ImVec2(float(tw), float(th));
        io.DisplayFramebufferScale = ImVec2(1, 1);
        static double last;
        double t = Now();
        io.DeltaTime = last > 0 ? float(std::max(1e-4, std::min(0.1, t - last))) : 1 / 60.0f;
        last = t;
    }
#ifdef XSG_GPU_SDL
    else if (g_win) {
        ImGui_ImplSDL3_NewFrame();
        fb = io.DisplayFramebufferScale.y > 0 ? io.DisplayFramebufferScale.y : 1.0f;
    }
#endif
    if (io.DisplaySize.x <= 0 || io.DisplaySize.y <= 0) return;
    float scale = MenuScale(io.DisplaySize.y);
    if (g_font_scale != scale * fb) FontsUp(scale, fb);
    if (g_style_scale != scale) {
        ImGui::GetStyle() = g_base;
        ImGui::GetStyle().ScaleAllSizes(scale);
        g_style_scale = scale;
    }
    if (g_reset) {
        io.ClearInputKeys();
        io.ClearInputMouse();
    }
    PadFrame();
    g_reset = false;
    if (g_tab_req >= 0) {
        g_tab = g_tab_req;
        g_tab_req = -1;
        opt_focus_first();
    }
    ImGui::NewFrame();
    xsgui::AnimFrame(Now());
    xsgui::ArtFrame();
    xsgui::AnimGc();
    if (menus)   // the game's menus, under the battle HUD and dialog
        xsgui::MenuDraw(ImGui::GetBackgroundDrawList(), ImGui::GetFont(), s.game_x / fb, s.game_y / fb, s.game_w / fb,
                        s.game_h / fb);
    if (battle)
        xsgui::BattleDraw(ImGui::GetBackgroundDrawList(), ImGui::GetFont(), s.game_x / fb, s.game_y / fb, s.game_w / fb,
                          s.game_h / fb, vsync);
    if (dialog)   // the rebuilt game UI (port/gpu/ui/), under the menu
        xsgui::DialogDraw(ImGui::GetBackgroundDrawList(), ImGui::GetFont(), s.game_x / fb, s.game_y / fb, s.game_w / fb,
                          s.game_h / fb, Now());
    if (g_open) LayoutMenu(scale);
    OverlayDraw(scale);
    ImGui::Render();
    g_visible = ImGui::GetDrawData()->TotalVtxCount > 0;
    if (!g_test.shot.empty()) {
        // advance after this frame's shot
        int next = g_test.page + 1;
        if (next < TAB_COUNT) g_tab_req = next;
        else if (next == TAB_COUNT) SetOpen(false);
        if (next > TAB_COUNT) {
            next = -1;
            g_overlay = g_test.saved_overlay;
            Log("test hook: done");
        }
        g_test.page = next;
    }
}

bool Visible() {
    if (!g_ok) return false;
    ImDrawData* dd = ImGui::GetDrawData();
    return g_visible && dd && dd->Valid;
}

void Draw(Vulkan::CommandBuffer& cmd, uint32_t w, uint32_t h) {
    if (!Visible()) return;
    RenderDraw(cmd, ImGui::GetDrawData(), w, h);
}

std::string TestShot(uint32_t* w, uint32_t* h) {
    *w = g_test.w;
    *h = g_test.h;
    return g_test.shot;
}

bool TestRunning(uint32_t* w, uint32_t* h) {
    *w = g_test.w;
    *h = g_test.h;
    return g_test.page >= 0 || (g_test.at && true);
}

}  // namespace xsgmenu

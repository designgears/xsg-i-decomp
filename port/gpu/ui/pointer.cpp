/* The native mouse, xsg-gpu's side (pointer.h). */
#include "pointer.h"

#include "layout.h"
#include "gpu_ipc.h"
#include "../menu/cfg.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace xsgui {
namespace {

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

GpuMouse* g_shm;
ImVec2 g_pos(-1e9f, -1e9f);
bool g_inside;
uint32_t g_held;
uint32_t g_down[5], g_up[5];
int g_pending_left;           // left presses not yet decided (settings hint or game)
float g_wheel_acc;
int32_t g_wheel;
uint32_t g_moves;
double g_move_t = -1e9;

struct Hit { ImVec2 a, b; int kind, index; };
std::vector<Hit> g_hits;
std::vector<std::pair<ImVec2, ImVec2>> g_blocks;
bool g_over_ui;
bool g_have_settings;
ImVec2 g_set_a, g_set_b;

bool g_hot;                   // over something clickable (this frame)
Hit g_last_hit{{}, {}, 0, 0}; // last frame's hit under the pointer (kind 0: none)
bool g_last_pressed;          // ... with the left button held
// for the native renderer (native.cpp Live, on this same thread)
bool g_game_on;               // the pointer is in the game picture with the mouse on
int g_gx, g_gy;               // the pointer, game px (centre anchor; negative / past 512 in the wide margins)
int g_out_sel, g_out_hov;
uint32_t g_hov_mask[8];
int g_pick = -1;
int g_pick_age;

bool In(ImVec2 p, ImVec2 a, ImVec2 b) { return p.x >= a.x && p.x < b.x && p.y >= a.y && p.y < b.y; }

int Index(int button) {   // SDL_BUTTON_* -> GpuMouse bit
    switch (button) {
    case 1: return 0;   // left
    case 3: return 1;   // right
    case 2: return 2;   // middle
    case 4: return 3;   // back
    case 5: return 4;   // forward
    default: return -1;
    }
}

}  // namespace

void PointerInit(void* shm_base) {
    g_shm = shm_base ? reinterpret_cast<GpuMouse*>(static_cast<uint8_t*>(shm_base) + XSG_GPU_MOUSE_OFF) : nullptr;
}

void PointerMove(float x, float y) {
    // a real move shows the pointer again; 1 px jitter does not
    if (std::fabs(x - g_pos.x) >= 2.0f || std::fabs(y - g_pos.y) >= 2.0f || !g_inside) g_move_t = Now();
    g_pos = ImVec2(x, y);
    g_inside = true;
    g_moves++;
}
void PointerHide() { g_move_t = -1e9; }
void PointerButton(int button, bool down) {
    g_move_t = Now();
    const int i = Index(button);
    if (i < 0) return;
    if (down) {
        g_held |= 1u << i;
        if (i == 0) g_pending_left++;   // decided in PointerEnd (the settings hint takes it)
        else g_down[i]++;
    } else if (g_held & (1u << i)) {   // a release of a press the menu took is the menu's
        g_held &= ~(1u << i);
        g_up[i]++;
    }
}
void PointerWheel(float dy) {
    g_move_t = Now();
    g_wheel_acc += dy;
    while (g_wheel_acc >= 1.0f) g_wheel_acc -= 1.0f, g_wheel++;
    while (g_wheel_acc <= -1.0f) g_wheel_acc += 1.0f, g_wheel--;
}
void PointerLeave() { g_inside = false; }
void PointerFocusLost() {
    g_inside = false;
    for (int i = 0; i < 5; ++i)
        if (g_held & (1u << i)) g_up[i]++;   // released elsewhere: release them here
    g_held = 0;
}

void PointerBegin() {
    g_hits.clear();
    g_blocks.clear();
    g_have_settings = false;
}
void PointerBlock(ImVec2 a, ImVec2 b) { g_blocks.push_back({ImVec2(std::min(a.x, b.x), std::min(a.y, b.y)), ImVec2(std::max(a.x, b.x), std::max(a.y, b.y))}); }
void PointerHit(ImVec2 a, ImVec2 b, int kind, int index) { g_hits.push_back({a, b, kind, index}); }
void PointerSettings(ImVec2 a, ImVec2 b) {
    g_have_settings = true;
    g_set_a = a;
    g_set_b = b;
}

bool PointerHot() { return g_hot; }
bool PointerGame(int* x, int* y) {
    *x = g_gx;
    *y = g_gy;
    return g_game_on && !g_over_ui;   // over a UI panel: nothing in the scene is under the pointer
}
void PointerOutlineIds(int* s, int* h) {
    *s = g_out_sel;
    *h = g_out_hov;
}
void PointerHoverable(uint32_t* m) {
    for (int i = 0; i < 8; ++i) m[i] = g_hov_mask[i];
}
void PointerSetPick(int id) {
    g_pick = id;
    g_pick_age = 0;
}
bool PointerHovered(int kind, int index, bool* pressed) {
    const bool on = g_last_hit.kind == kind && g_last_hit.index == index;
    if (pressed) *pressed = on && g_last_pressed;
    return on;
}

bool PointerVisible() {
    const int hide = xsgcfg::GetInt("mouse.hide_after");
    return hide <= 0 || Now() - g_move_t < double(hide);
}

bool PointerEnd(ImDrawList* dl, float gx, float gy, float gw, float gh, bool rebuilt, bool blocked) {
    bool open_settings = false;
    const bool over_settings = g_inside && g_have_settings && In(g_pos, g_set_a, g_set_b);
    if (blocked) {
        g_pending_left = 0;   // clicks in the settings menu are the menu's
    } else {
        for (; g_pending_left > 0; --g_pending_left) {
            if (over_settings) open_settings = true;
            else g_down[0]++;
        }
    }
    // [mouse] enabled 0: nothing in the game reacts (only the "Settings" hints stay clickable)
    const bool on = xsgcfg::GetBool("mouse.enabled");
    const Hit* hit = nullptr;
    if (g_inside && on)
        for (auto& h : g_hits)
            if (In(g_pos, h.a, h.b)) hit = &h;
    (void)dl;
    // what the elements draw their hover / press state from next frame (the pointer shown, the menu closed)
    const bool live = PointerVisible() && g_inside && !blocked;
    if (live && hit) g_last_hit = *hit;
    else if (live && over_settings) g_last_hit = Hit{g_set_a, g_set_b, GPU_MOUSE_UI_SETTINGS, 0};
    else g_last_hit.kind = 0;
    g_last_pressed = live && (g_held & 1u);
    g_hot = live && (over_settings || hit);
    bool over_ui = over_settings || hit;
    if (g_inside)
        for (auto& bl : g_blocks)
            if (In(g_pos, bl.first, bl.second)) over_ui = true;
    g_over_ui = over_ui;
    if (!g_shm) return open_settings;

    // the game's report: what the pointer is over (the hand cursor) and outlines (units, people)
    {
        static uint32_t last_seq;
        static double last_t;
        static GpuMouse snap;   // a read that keeps meeting a write keeps the last good report
        bool ok = false;
        for (int tries = 0; tries < 100 && !ok; ++tries) {
            const uint32_t s0 = __atomic_load_n(&g_shm->g_seq, __ATOMIC_ACQUIRE);
            if (s0 & 1) continue;
            const uint32_t hot = g_shm->g_hot, o0 = g_shm->g_outline[0], o1 = g_shm->g_outline[1];
            uint32_t hm[8];
            for (int i = 0; i < 8; ++i) hm[i] = g_shm->g_hoverable[i];
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            ok = __atomic_load_n(&g_shm->g_seq, __ATOMIC_RELAXED) == s0;
            if (!ok) continue;
            snap.g_hot = hot, snap.g_outline[0] = int32_t(o0), snap.g_outline[1] = int32_t(o1);
            for (int i = 0; i < 8; ++i) snap.g_hoverable[i] = hm[i];
            if (s0 != last_seq) last_seq = s0, last_t = Now();
        }
        // the hand and the outlines: only for a pointer that shows (an idle, hidden one points at nothing)
        const bool fresh = Now() - last_t < 0.25 && live && on;
        if (fresh && snap.g_hot) g_hot = true;
        g_out_sel = fresh ? snap.g_outline[0] : 0;
        g_out_hov = fresh ? snap.g_outline[1] : 0;
        for (int i = 0; i < 8; ++i) g_hov_mask[i] = fresh ? snap.g_hoverable[i] : 0;
    }

    // the pointer in the game's space, per anchor
    int32_t x[3], y;
    if (rebuilt) {
        LayoutBegin(gx, gy, gw, gh);
        const int an[3] = {kLeft, kCentre, kRight};
        for (int i = 0; i < 3; ++i) x[i] = int32_t(std::lround(Unmap(g_pos, an[i]).x * 16));
        y = int32_t(std::lround(Unmap(g_pos, kCentre).y * 16));
    } else {
        const float px = gw > 0 ? (g_pos.x - gx) * 512.0f / gw : 0, py = gh > 0 ? (g_pos.y - gy) * 448.0f / gh : 0;
        x[0] = x[1] = x[2] = int32_t(std::lround(px * 16));
        y = int32_t(std::lround(py * 16));
    }
    const std::string field = xsgcfg::Get("mouse.field");
    const uint32_t active = on && g_inside && !blocked && gw > 0;
    GpuMouse* p = g_shm;
    const uint32_t s = p->seq;
    __atomic_store_n(&p->seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    p->active = active;
    for (int i = 0; i < 3; ++i) p->x[i] = x[i];
    p->y = y;
    p->held = g_held;
    for (int i = 0; i < 5; ++i) p->down[i] = g_down[i], p->up[i] = g_up[i];
    p->wheel = g_wheel;
    p->moves = g_moves;
    p->field = field == "hold" ? 1 : field == "click" ? 2 : 0;
    p->flags = (xsgcfg::GetBool("mouse.hover") ? 1u : 0u) | (xsgcfg::GetBool("mouse.click_advances") ? 2u : 0u);
    p->walk_radius = uint32_t(xsgcfg::GetInt("mouse.run_radius"));
    // the pointer for the native renderer's picking; a pick no frame refreshed lately is unknown
    g_game_on = active != 0;
    g_gx = x[1] >> 4;
    g_gy = y >> 4;
    if (g_pick_age++ > 20) g_pick = -1;
    p->pick = g_over_ui ? 0 : g_pick;
    p->ui_over = g_over_ui;
    p->ui_kind = hit ? hit->kind : 0;
    p->ui_index = hit ? hit->index : 0;
    __atomic_store_n(&p->seq, s + 2, __ATOMIC_RELEASE);
    return open_settings;
}

}  // namespace xsgui

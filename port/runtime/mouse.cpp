/* Native mouse, the game's side: xsg_mouse.h, port/docs/MOUSE.md. */
#include "xsg_mouse.h"
#include "gpu_ipc.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "xsg_rt.h"
extern void* xglStudioGetCamera2(int camera_id);
extern void xglSoundEffectNormalID(int sound_id, int variant);
extern char CfCameraDefine[];
extern char actor[];   // ActRecord actor[64] (src/main/act_1.h)
}

extern "C" void* xsg_gpu_shm_base(void);   // gpu_client.cpp

namespace {

// ActRecord (src/main/act_1.h)
constexpr int kActors = 64;
constexpr size_t kActStride = 0xa70;
constexpr size_t kActLive = 0x86;     // int16, nonzero: in use
constexpr size_t kActPos = 0x10;      // float x, y, z
constexpr size_t kActTalk = 0x9f4;    // its talk hook (game.c checkTalk)

constexpr uint32_t kHoverFrames = 20;   // "moved" lasts a third of a second
constexpr int kCarryFrames = 2;         // an unclaimed press stays offered this many more frames

XsgMouse g_m;
uint32_t g_frame = ~0u;
bool g_have;                 // a snapshot was read before
uint32_t g_down[5], g_up[5];
int32_t g_wheel_sum;
uint32_t g_moves;
uint32_t g_last_move;        // the frame the pointer last moved
int g_fresh;                 // presses new this frame (g_m.pressed adds the carried ones)
int g_carry, g_carry_rel;    // presses nobody claimed, offered again next frame (and their releases)
int g_carry_age[5];
int g_claimed;               // presses claimed this frame
int g_xa[3];                 // x per anchor (left, centre, right)
int g_log = -1;
bool g_hot;                  // this frame: over something clickable
int g_outline[2];
uint32_t g_hoverable[8];   // ids (bit = id - 1): actors, then hooks' objects
bool g_list_seen, g_list_seen_prev;   // a menu had the pointer this frame / the last (xsg_mouse_capture)

// pad taps: masks waiting, one in flight (pressed for 2 reads, released for 2)
unsigned g_tap_q[16];
int g_tap_n;
unsigned g_tap_mask;
int g_tap_left;

// hand cursor: this frame's brightest, and last frame's
int g_hand_x, g_hand_y, g_hand_a = -1, g_hand_depth;
int g_last_hand_x, g_last_hand_y;
bool g_last_hand;

bool Log() {
    if (g_log < 0) g_log = std::getenv("XSG_MOUSE_LOG") != nullptr;
    return g_log != 0;
}

void Tap(unsigned m) {
    if (g_tap_n < 16) g_tap_q[g_tap_n++] = m;
    if (Log()) std::fprintf(stderr, "[mouse] tap %04x (frame %u)\n", m, unsigned(xsg_frame));
}

const uint8_t* Actor(int i) { return reinterpret_cast<const uint8_t*>(actor) + size_t(i) * kActStride; }
const float* ActorPos(int i) { return reinterpret_cast<const float*>(Actor(i) + kActPos); }
// a person the player can talk to, on the player's floor
bool Talkable(int i, const float* player) {
    const uint8_t* a = Actor(i);
    return *reinterpret_cast<const int16_t*>(a + kActLive) && *reinterpret_cast<const int32_t*>(a + kActTalk) &&
           std::fabs(ActorPos(i)[1] - player[1]) <= 2.0f;
}

// The frame that just ended: a rebuilt prompt under a new left press presses its button (even
// over a list); presses nobody claimed are offered again for kCarryFrames frames (the game's
// logic may run every other vblank), then become pad buttons.
void EndFrame() {
    int pressed = g_m.pressed;
    if ((g_fresh & XSG_MB_LEFT) && g_m.ui_kind == XSG_UI_PROMPT && g_m.ui_index) {
        Tap(unsigned(g_m.ui_index));
        pressed &= ~XSG_MB_LEFT;
    }
    // a left click on nothing in an open menu does nothing (it must not confirm its selection)
    const int free = pressed & ~(g_claimed | (g_list_seen ? XSG_MB_LEFT : 0));
    int fall = 0;
    g_carry = 0;
    for (int i = 0; i < 5; ++i) {
        const int b = 1 << i;
        if (!(free & b)) g_carry_age[i] = 0;
        else if (g_carry_age[i] < kCarryFrames) g_carry |= b, g_carry_age[i]++;
        else fall |= b, g_carry_age[i] = 0;
    }
    g_carry_rel = g_m.released & g_carry;
    if ((fall & XSG_MB_LEFT) && g_m.advances) Tap(XSG_PAD_CIRCLE);   // advance text, talk, confirm
    if (fall & XSG_MB_RIGHT) Tap(XSG_PAD_CROSS);                     // back
    if (fall & XSG_MB_MIDDLE) Tap(XSG_PAD_TRIANGLE);
    if (fall & XSG_MB_BACK) Tap(XSG_PAD_L1);
    if (fall & XSG_MB_FORWARD) Tap(XSG_PAD_R1);
    for (int i = 0; i < g_m.wheel && i < 4; ++i) Tap(XSG_PAD_UP);
    for (int i = 0; i < -g_m.wheel && i < 4; ++i) Tap(XSG_PAD_DOWN);
}

void Publish() {
    void* const base = xsg_gpu_shm_base();
    if (!base) return;
    GpuMouse* p = reinterpret_cast<GpuMouse*>(static_cast<uint8_t*>(base) + XSG_GPU_MOUSE_OFF);
    const uint32_t s = p->g_seq;
    __atomic_store_n(&p->g_seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    p->g_hot = g_hot;
    p->g_outline[0] = g_outline[0];
    p->g_outline[1] = g_outline[1];
    for (int i = 0; i < 8; ++i) p->g_hoverable[i] = g_hoverable[i];
    __atomic_store_n(&p->g_seq, s + 2, __ATOMIC_RELEASE);
}

bool Read(GpuMouse* s) {
    void* const base = xsg_gpu_shm_base();
    if (!base) return false;
    const GpuMouse* p = reinterpret_cast<const GpuMouse*>(static_cast<const uint8_t*>(base) + XSG_GPU_MOUSE_OFF);
    for (int tries = 0; tries < 1000; ++tries) {
        const uint32_t s0 = __atomic_load_n(&p->seq, __ATOMIC_ACQUIRE);
        if (s0 & 1) continue;
        std::memcpy(s, const_cast<const GpuMouse*>(p), sizeof *s);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&p->seq, __ATOMIC_RELAXED) == s0) return true;
    }
    return false;
}

void SyncCounters(const GpuMouse& s) {
    for (int i = 0; i < 5; ++i) g_down[i] = s.down[i], g_up[i] = s.up[i];
    g_wheel_sum = s.wheel;
    g_moves = s.moves;
}

void Update() {
    if (g_frame == xsg_frame) return;
    if (g_frame != ~0u) {
        EndFrame();
        Publish();
    }
    g_hot = false;
    g_outline[0] = g_outline[1] = 0;
    std::memset(g_hoverable, 0, sizeof g_hoverable);
    g_list_seen_prev = g_list_seen;
    g_list_seen = false;
    g_last_hand = g_hand_a >= 0;
    g_last_hand_x = g_hand_x;
    g_last_hand_y = g_hand_y;
    g_hand_a = -1;
    g_frame = xsg_frame;
    g_claimed = 0;
    g_fresh = 0;
    GpuMouse s;
    if (!Read(&s)) {   // xsg-gpu gone, or its write kept meeting this read: last frame's pointer, no new events
        if (!xsg_gpu_shm_base()) std::memset(&g_m, 0, sizeof g_m);
        g_m.pressed = g_m.active ? g_carry : 0;
        g_m.released = g_m.active ? g_carry_rel : 0;
        g_m.wheel = 0;
        return;
    }
    std::memset(&g_m, 0, sizeof g_m);
    if (!g_have) {   // the first snapshot: counters as they are, no events
        SyncCounters(s);
        g_have = true;
    }
    if (!s.active) {   // counters still advance, so nothing pressed meanwhile replays later
        SyncCounters(s);
        g_carry = g_carry_rel = 0;
        return;
    }
    g_m.active = 1;
    for (int i = 0; i < 3; ++i) g_xa[i] = s.x[i] >> 4;
    g_m.x = g_xa[1];
    g_m.y = s.y >> 4;
    g_m.held = int(s.held);
    for (int i = 0; i < 5; ++i) {
        if (s.down[i] != g_down[i]) g_fresh |= 1 << i;
        if (s.up[i] != g_up[i]) g_m.released |= 1 << i;
    }
    g_m.pressed = g_fresh | g_carry;
    g_m.released |= g_carry_rel;
    g_m.wheel = s.wheel - g_wheel_sum;
    // "moved": within the last third of a second, so a menu that takes input every other frame
    // (or opens a moment later) still follows a pointer that just moved
    if (s.moves != g_moves) g_last_move = xsg_frame;
    if (s.wheel != g_wheel_sum) g_last_move = xsg_frame - 1000;   // the wheel moved the cursor: hover waits for motion
    g_m.moved = xsg_frame - g_last_move < kHoverFrames;
    SyncCounters(s);
    g_m.hover = (s.flags & 1) != 0;
    g_m.advances = (s.flags & 2) != 0;
    g_m.field = int(s.field);
    g_m.run_radius = int(s.walk_radius ? s.walk_radius : 96);
    g_m.pick = s.pick;
    g_m.over_ui = int(s.ui_over);
    if (Log()) {
        static int lp = -99;
        if (s.pick != lp) lp = s.pick, std::fprintf(stderr, "[mouse] pick %d (frame %u)\n", s.pick, unsigned(xsg_frame));
    }
    g_m.ui_kind = s.ui_kind;
    g_m.ui_index = s.ui_index;
    if (Log() && (g_m.pressed || g_m.wheel))
        std::fprintf(stderr, "[mouse] frame %u at %d,%d pressed %x (carried %x) wheel %d ui %d/%d\n", unsigned(xsg_frame), g_m.x,
                     g_m.y, g_m.pressed, g_carry, g_m.wheel, g_m.ui_kind, g_m.ui_index);
}

// --- field: the camera's projection (MMathRotTransPers: row vector * viewMatrix, /w, *scale + offset)
struct V2 { float x, y; bool ok; };
V2 Project(const uint8_t* cam, float x, float y, float z) {
    const float* m = reinterpret_cast<const float*>(cam + 0x470);
    const float* off = reinterpret_cast<const float*>(cam + 0x70);
    const float* sc = reinterpret_cast<const float*>(cam + 0x80);
    const float v[4] = {x, y, z, 1.0f};
    float r[4];
    for (int c = 0; c < 4; ++c) r[c] = v[0] * m[c] + v[1] * m[4 + c] + v[2] * m[8 + c] + v[3] * m[12 + c];
    if (!(r[3] > 1e-5f || r[3] < -1e-5f)) return {0, 0, false};
    // GS coordinates: 2048 is the frame's centre and scale (2048, -1792) already gives pixels -> 512 x 448
    const float gx = r[0] / r[3] * sc[0] + off[0] - 2048.0f + 256.0f, gy = r[1] / r[3] * sc[1] + off[1] - 2048.0f + 224.0f;
    return {gx, gy, std::isfinite(gx) && std::isfinite(gy)};
}

// click-to-walk / walk-and-talk state
struct Walk {
    bool on = false;
    float tx = 0, tz = 0;    // world target
    bool talk = false;       // tap CIRCLE on arrival
    uint32_t start = 0, best_t = 0;
    float best = 1e9f;
} g_walk;
uint32_t g_press_frame;
bool g_pressing;
bool g_dragged;

// the talkable actor under the pointer (game.c checkTalk's tests, loosely): each one's
// feet-to-head column on screen, 14 px either side; the nearest to the camera wins.  -1: none.
int TalkableAt(const uint8_t* cam, const float* player, float px, float py, float* out) {
    int best = -1;
    float best_d = 1e9f;
    for (int i = 0; i < kActors; ++i) {
        if (!Talkable(i, player)) continue;
        const float* q = ActorPos(i);
        const V2 f = Project(cam, q[0], q[1], q[2]), h = Project(cam, q[0], q[1] + 1.7f, q[2]);
        if (!f.ok || !h.ok) continue;
        const float top = std::fmin(f.y, h.y) - 4, bot = std::fmax(f.y, h.y) + 6;
        if (py < top || py > bot) continue;
        const float t = (bot - top) > 1 ? (py - top) / (bot - top) : 0;
        const float cx = h.x + (f.x - h.x) * t;
        if (std::fabs(px - cx) > 14) continue;
        const float dx = q[0] - player[0], dz = q[2] - player[2], d = dx * dx + dz * dz;
        if (d < best_d) best_d = d, best = i, out[0] = q[0], out[1] = q[2];
    }
    return best;
}

// the talkable actor under the pointer: by the native renderer's object ids (the model's own
// pixels), or without them by its column on screen
int TalkablePick(const uint8_t* cam, const float* player, float* out) {
    if (g_m.over_ui) return -1;
    if (g_m.pick >= 0) {
        const int i = g_m.pick - 1;
        if (i < 0 || i >= kActors || !Talkable(i, player)) return -1;
        out[0] = ActorPos(i)[0], out[1] = ActorPos(i)[2];
        return i;
    }
    return TalkableAt(cam, player, float(g_m.x), float(g_m.y), out);
}

int g_cur_tag;                    // object ids: the actor (+ 1) whose models are entered now
unsigned char g_slot_tag[512];    // ... and of each model slot this frame (act_2.c, nml_model_set.c)

}  // namespace

extern "C" {

void xsg_mouse_hot(void) { Update(); g_hot = true; }

int xsg_mouse_actor_index(const void* a) {
    const std::ptrdiff_t off = static_cast<const char*>(a) - actor;
    return (a && off >= 0 && off < std::ptrdiff_t(kActors * kActStride) && off % std::ptrdiff_t(kActStride) == 0)
               ? int(off / std::ptrdiff_t(kActStride))
               : -1;
}
void xsg_mouse_model_actor(const void* a) { g_cur_tag = xsg_mouse_actor_index(a) + 1; }
void xsg_mouse_object_begin(int id) { g_cur_tag = id >= XSG_OBJ_FIRST && id <= XSG_OBJ_LAST ? id : 0; }
void xsg_mouse_object_end(void) { g_cur_tag = 0; }

int xsg_mouse_object_pick(const int* ids, int n, int sel) {
    Update();
    if (!g_m.active || n <= 0) return sel;
    g_list_seen = true;   // a click off the objects does nothing
    int best = -1;
    if (!g_m.over_ui && g_m.pick > 0)
        for (int i = 0; i < n; ++i)
            if (ids[i] > 0 && ids[i] == g_m.pick) best = i;
    for (int i = 0; i < n; ++i)   // the renderer outlines these under the pointer at once
        if (ids[i] > 0) xsg_mouse_hoverable(ids[i] - 1);
    g_outline[0] = sel >= 0 && sel < n && ids[sel] > 0 ? ids[sel] : 0;
    g_outline[1] = best >= 0 && best != sel ? ids[best] : 0;
    if (best < 0) return sel;
    g_hot = true;
    if (xsg_mouse_click(XSG_MB_LEFT)) {
        if (Log()) std::fprintf(stderr, "[mouse] object %d (choice %d of %d) clicked, current %d\n", ids[best], best, n, sel);
        if (best == sel) Tap(XSG_PAD_CIRCLE);   // the current one again: confirm
        return best;
    }
    return best != sel && g_m.hover && g_m.moved ? best : sel;
}
void xsg_mouse_model_slot(int slot) {
    if (slot >= 0 && slot < 512) g_slot_tag[slot] = static_cast<unsigned char>(g_cur_tag);
}
int xsg_mouse_model_tag(int slot) { return slot >= 0 && slot < 512 ? g_slot_tag[slot] : 0; }

void xsg_mouse_outline(int selected_actor, int hovered_actor) {
    Update();
    g_outline[0] = selected_actor >= 0 ? selected_actor + 1 : 0;
    g_outline[1] = hovered_actor >= 0 ? hovered_actor + 1 : 0;
}
void xsg_mouse_hoverable(int actor_index) {
    Update();
    if (actor_index >= 0 && actor_index < 256) g_hoverable[actor_index >> 5] |= 1u << (actor_index & 31);
}

const XsgMouse* xsg_mouse(void) {
    Update();
    return &g_m;
}

int xsg_mouse_x(int x0, int x1) {
    Update();
    // layout.cpp AnchorFor / Map: stretch (each point by its own half) / left third / centre / right third
    if (x1 - x0 >= 440 || (x0 <= 8 && x1 >= 504)) return g_xa[0] < 256 ? g_xa[0] : g_xa[2];
    const int c = (x0 + x1) / 2;
    return c < 512 / 3 ? g_xa[0] : c > 1024 / 3 ? g_xa[2] : g_xa[1];
}

int xsg_mouse_in(int x, int y, int w, int h) {
    Update();
    if (!g_m.active) return 0;
    const int px = xsg_mouse_x(x, x + w);
    const bool in = px >= x && px < x + w && g_m.y >= y && g_m.y < y + h;
    if (in) g_hot = true;
    return in;
}

int xsg_mouse_click(int button) {
    Update();
    if (!(g_m.pressed & button) || (g_claimed & button)) return 0;
    g_claimed |= button;
    return 1;
}

void xsg_mouse_claim(int button) {
    Update();
    g_claimed |= button;
}

void xsg_mouse_capture(void) {
    Update();
    if (g_m.active) g_list_seen = true;
}

void xsg_mouse_tap(unsigned mask) { Tap(mask); }

int xsg_mouse_grid(int x, int y, int w, int pitch, int cols, int first_row, int visible, int count, int sel, int* activate) {
    Update();
    if (activate) *activate = 0;
    if (!g_m.active || pitch <= 0 || count <= 0 || cols <= 0 || w <= 0) return sel;
    g_list_seen = true;
    const int px = xsg_mouse_x(x, x + w);
    if (px < x || px >= x + w || g_m.y < y) return sel;
    const int r = (g_m.y - y) / pitch, c = (px - x) * cols / w;
    if (r >= visible || c >= cols) return sel;
    const int item = (first_row + r) * cols + c;
    if (item < 0 || item >= count) return sel;
    g_hot = true;
    if (xsg_mouse_click(XSG_MB_LEFT)) {
        if (activate) *activate = 1;
        if (Log()) std::fprintf(stderr, "[mouse] grid %d,%d %dx%d (%d cols): click on item %d of %d\n", x, y, w, pitch, cols, item, count);
        return item;
    }
    if (g_m.moved && g_m.hover) return item;
    return sel;
}

int xsg_mouse_list(int x, int y, int w, int pitch, int first, int visible, int count, int sel, int* activate) {
    return xsg_mouse_grid(x, y, w, pitch, 1, first, visible, count, sel, activate);
}

int xsg_mouse_rects(const short (*r)[4], int n, int sel) {
    Update();
    if (!g_m.active) return sel;
    g_list_seen = true;   // a screen of choices has the pointer: a click off them does nothing
    int hit = -1;
    for (int i = 0; i < n; ++i)
        if (r[i][2] > 0 && xsg_mouse_in(r[i][0], r[i][1], r[i][2], r[i][3])) hit = i;
    if (hit < 0) return sel;
    g_hot = true;
    if (xsg_mouse_click(XSG_MB_LEFT)) {
        if (Log()) std::fprintf(stderr, "[mouse] rects: click on %d of %d\n", hit, n);
        Tap(XSG_PAD_CIRCLE);
        return hit;
    }
    return g_m.moved && g_m.hover ? hit : sel;
}

int xsg_mouse_grid_pick(int x, int y, int w, int pitch, int cols, int first_row, int visible, int count, int sel) {
    int act = 0;
    const int ns = xsg_mouse_grid(x, y, w, pitch, cols, first_row, visible, count, sel, &act);
    if (ns != sel) xglSoundEffectNormalID(3, 0);
    if (act) Tap(XSG_PAD_CIRCLE);   // confirm through the caller's own CIRCLE
    return ns;
}

void xsg_mouse_hand(int x, int y, int alpha, int depth) {
    Update();
    if (alpha < 0x40 || alpha < g_hand_a) return;
    if (x < 0 || x >= 512 || y < 0 || y >= 448) return;   // parked off screen (a window slid away)
    if (alpha == g_hand_a && depth > g_hand_depth) return;  // a popup's hand over a list's parked one
    g_hand_a = alpha;
    g_hand_depth = depth;
    g_hand_x = x;
    g_hand_y = y;
}

int xsg_mouse_last_hand(int* x, int* y) {
    Update();
    if (!g_last_hand) return 0;
    *x = g_last_hand_x;
    *y = g_last_hand_y;
    return 1;
}

unsigned xsg_mouse_pad_buttons(void) {
    Update();
    if (!g_tap_left && g_tap_n) {
        g_tap_mask = g_tap_q[0];
        std::memmove(g_tap_q, g_tap_q + 1, sizeof(unsigned) * size_t(--g_tap_n));
        g_tap_left = 4;
    }
    if (!g_tap_left) return 0;
    return g_tap_left-- > 2 ? g_tap_mask : 0;
}

void xsg_mouse_pad_used(void) {
    Update();
    g_last_move = xsg_frame - 1000;   // the keyboard / pad moved the cursor: hover waits for motion
    g_m.moved = 0;
}

void xsg_mouse_field_cancel(void) {
    g_walk.on = false;
    g_pressing = false;
}

int xsg_mouse_field_stick(const float* pos, signed char* sx, signed char* sy) {
    Update();
    if (!g_m.active || !g_m.field) { xsg_mouse_field_cancel(); return 0; }
    // a menu list live now or in the last frame has the pointer (a choice over the field)
    if (g_list_seen || g_list_seen_prev) { xsg_mouse_field_cancel(); return 0; }
    const uint8_t* cam = static_cast<const uint8_t*>(xglStudioGetCamera2(0));
    if (!cam) return 0;
    const float yaw = *reinterpret_cast<const float*>(CfCameraDefine + 0x34);
    // the ground's screen Jacobian at the player (a metre along x and z)
    const V2 p0 = Project(cam, pos[0], pos[1], pos[2]);
    const V2 px = Project(cam, pos[0] + 1, pos[1], pos[2]);
    const V2 pz = Project(cam, pos[0], pos[1], pos[2] + 1);
    if (!p0.ok || !px.ok || !pz.ok) return 0;
    const float a = px.x - p0.x, b = pz.x - p0.x, c = px.y - p0.y, d = pz.y - p0.y;
    const float det = a * d - b * c;
    if (std::fabs(det) < 1e-4f) return 0;
    // the pointer (centre anchor: the picture) relative to the player's feet, in world x / z
    const float mx = float(g_m.x) - p0.x, my = float(g_m.y) - p0.y;
    const float wx = (d * mx - b * my) / det, wz = (-c * mx + a * my) / det;
    const float sdist = std::sqrt(mx * mx + my * my);
    for (int i = 0; i < kActors; ++i)   // people the renderer outlines at once under the pointer
        if (Talkable(i, pos)) g_hoverable[i >> 5] |= 1u << (i & 31);
    {   // a person under the pointer: the hand cursor and an outline around the model
        float who[2];
        const int t = TalkablePick(cam, pos, who);
        if (t >= 0) {
            g_hot = true;
            g_outline[1] = t + 1;
        }
    }

    // press / release: a short click goes somewhere (or talks), holding walks
    if ((g_m.pressed & XSG_MB_LEFT) && !g_m.over_ui && !(g_claimed & XSG_MB_LEFT)) {
        g_claimed |= XSG_MB_LEFT;
        g_pressing = true;
        g_dragged = false;
        g_press_frame = xsg_frame;
        g_walk.on = false;
    }
    if (g_pressing && (g_m.held & XSG_MB_LEFT) && (xsg_frame - g_press_frame > 10)) g_dragged = true;
    if (g_pressing && (g_m.released & XSG_MB_LEFT)) {
        g_pressing = false;
        if (!g_dragged) {
            float tx = pos[0] + wx, tz = pos[2] + wz, who[2];
            const bool npc = TalkablePick(cam, pos, who) >= 0;
            if (npc) tx = who[0], tz = who[1];
            if (g_m.field == 1 && !npc) {
                Tap(XSG_PAD_CIRCLE);   // hold mode: a click examines / talks in front
            } else {
                g_walk.on = true;
                g_walk.tx = tx, g_walk.tz = tz;
                g_walk.talk = npc;
                g_walk.start = g_walk.best_t = xsg_frame;
                g_walk.best = 1e9f;
                if (Log()) std::fprintf(stderr, "[mouse] walk to %.2f,%.2f%s\n", tx, tz, npc ? " (talk)" : "");
            }
        }
    }
    float dx, dz, mag;
    if (g_pressing && g_dragged && (g_m.held & XSG_MB_LEFT)) {   // hold: toward the pointer
        if (sdist < 6.0f) { *sx = 0, *sy = 0; return 1; }
        dx = wx, dz = wz;
        mag = sdist < float(g_m.run_radius) ? 72.0f : 127.0f;
    } else if (g_walk.on) {
        dx = g_walk.tx - pos[0], dz = g_walk.tz - pos[2];
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist < g_walk.best - 0.05f) g_walk.best = dist, g_walk.best_t = xsg_frame;
        const float arrive = g_walk.talk ? 0.95f : 0.35f;
        if (dist < arrive || xsg_frame - g_walk.best_t > 40 || xsg_frame - g_walk.start > 600) {
            if (Log()) std::fprintf(stderr, "[mouse] walk ends: %.2f away after %u frames\n", dist, unsigned(xsg_frame - g_walk.start));
            if (g_walk.talk && dist < 1.6f) Tap(XSG_PAD_CIRCLE);
            g_walk.on = false;
            return 0;
        }
        mag = dist > 2.5f ? 127.0f : 72.0f;
    } else {
        return 0;
    }
    // world direction -> the stick (GameCfPlayerMove: angle = atan2(x, y) + yaw)
    const float ang = std::atan2(dx, dz) - yaw;
    if (Log() && xsg_frame % 15 == 0)
        std::fprintf(stderr, "[mouse] field: player %.2f,%.2f,%.2f at %.0f,%.0f (J %.1f %.1f %.1f %.1f) d %.2f,%.2f yaw %.2f ang %.2f\n",
                     pos[0], pos[1], pos[2], p0.x, p0.y, a, b, c, d, dx, dz, yaw, ang);
    *sx = static_cast<signed char>(std::lround(std::sin(ang) * mag));
    *sy = static_cast<signed char>(std::lround(std::cos(ang) * mag));
    return 1;
}

}  // extern "C"

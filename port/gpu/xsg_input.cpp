/*
 * Stage 10: keyboard and gamepad input for the 64-bit xsg-gpu process.
 *
 * xsg-gpu owns the SDL3 window, so it also owns input.  Pad state is kept
 * from SDL events alone (key down/up, gamepad button/axis), never from
 * SDL_GetKeyboardState, so synthetic events pushed by the XSG_GPU_INPUT_TEST
 * hook go through exactly the same path as real ones.  Once per game vblank
 * the state is published as a DualShock 2 report (buttons, 4 stick bytes,
 * 12 pressure bytes) in GpuPad (gpu_ipc.h); the game's scePadRead
 * (port/runtime/sdk.c) reads it and ORs in its XSG_PAD script.
 *
 * Config: the [keyboard] / [gamepad] / [video] sections of
 * ~/.config/xsg/settings.ini (menu/cfg.cpp; input.ini is migrated into it).
 * Format and defaults: port/docs/STAGE10_INPUT.md, port/docs/MENU.md.
 * While the in-game menu is open the game gets a neutral pad.
 */
#include "xsg_input.h"
#include "menu/cfg.h"

#ifdef XSG_GPU_SDL
#include <SDL3/SDL.h>
#endif

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace xsginput {
namespace {

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[xsg-gpu] input: %s\n", b);
}

/* SCE_PAD masks (low 16 bits) plus keyboard-to-stick pseudo targets. */
enum : uint32_t {
    L2 = 0x0001, R2 = 0x0002, L1 = 0x0004, R1 = 0x0008, TRI = 0x0010, CIR = 0x0020, CRO = 0x0040, SQU = 0x0080,
    SEL = 0x0100, L3 = 0x0200, R3 = 0x0400, STA = 0x0800, UP = 0x1000, RIGHT = 0x2000, DOWN = 0x4000, LEFT = 0x8000,
    LS_UP = 1u << 16, LS_DOWN = 1u << 17, LS_LEFT = 1u << 18, LS_RIGHT = 1u << 19,
    RS_UP = 1u << 20, RS_DOWN = 1u << 21, RS_LEFT = 1u << 22, RS_RIGHT = 1u << 23,
};
const struct { const char* name; uint32_t mask; } kTargets[] = {
    {"CROSS", CRO}, {"X", CRO}, {"CIRCLE", CIR}, {"O", CIR}, {"SQUARE", SQU}, {"TRIANGLE", TRI},
    {"L1", L1}, {"R1", R1}, {"L2", L2}, {"R2", R2}, {"L3", L3}, {"R3", R3},
    {"START", STA}, {"SELECT", SEL}, {"UP", UP}, {"DOWN", DOWN}, {"LEFT", LEFT}, {"RIGHT", RIGHT},
    {"LS_UP", LS_UP}, {"LS_DOWN", LS_DOWN}, {"LS_LEFT", LS_LEFT}, {"LS_RIGHT", LS_RIGHT},
    {"RS_UP", RS_UP}, {"RS_DOWN", RS_DOWN}, {"RS_LEFT", RS_LEFT}, {"RS_RIGHT", RS_RIGHT},
    {"NONE", 0},
};
/* DS2 pressure byte order. */
const uint32_t kPressBit[12] = {RIGHT, LEFT, UP, DOWN, TRI, CIR, CRO, SQU, L1, R1, L2, R2};

std::string Upper(std::string s) {
    for (auto& c : s) c = char(std::toupper((unsigned char)c));
    return s;
}
std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
/* "CROSS+UP" -> mask; false on an unknown name. */
bool ParseTargets(const std::string& v, uint32_t* out) {
    uint32_t m = 0;
    size_t i = 0;
    std::string s = Upper(v);
    while (i <= s.size()) {
        size_t j = s.find('+', i);
        if (j == std::string::npos) j = s.size();
        std::string t = Trim(s.substr(i, j - i));
        bool ok = false;
        for (auto& k : kTargets)
            if (t == k.name) { m |= k.mask; ok = true; break; }
        if (!ok) return false;
        i = j + 1;
    }
    *out = m;
    return true;
}

GpuPad* g_pad;
bool g_sdl;
bool g_menu_open;
int g_deadzone = 8000;
int g_trigger = 8000;

#ifdef XSG_GPU_SDL
std::map<SDL_Scancode, uint32_t> g_keymap;
std::map<int, uint32_t> g_padmap;          // SDL_GamepadButton -> mask
uint32_t g_lt = L2, g_rt = R2;             // trigger axes -> mask
constexpr int kLT = SDL_GAMEPAD_BUTTON_COUNT, kRT = SDL_GAMEPAD_BUTTON_COUNT + 1;   // pseudo buttons (binds API)
bool g_keys[SDL_SCANCODE_COUNT];
struct PadState {
    SDL_Gamepad* gp = nullptr;
    bool btn[SDL_GAMEPAD_BUTTON_COUNT] = {};
    int axis[SDL_GAMEPAD_AXIS_COUNT] = {};
};
std::map<SDL_JoystickID, PadState> g_pads;

/* SDL's own names plus the positional ones (its string table spells the
   face buttons a/b/x/y even though they mean south/east/west/north). */
SDL_GamepadButton ButtonFromName(std::string n) {
    for (auto& c : n) c = char(std::tolower((unsigned char)c));
    if (n == "south") n = "a";
    else if (n == "east") n = "b";
    else if (n == "west") n = "x";
    else if (n == "north") n = "y";
    return SDL_GetGamepadButtonFromString(n.c_str());
}

void DefaultMaps() {
    auto K = [](SDL_Scancode s, uint32_t m) { g_keymap[s] = m; };
    K(SDL_SCANCODE_UP, UP); K(SDL_SCANCODE_DOWN, DOWN); K(SDL_SCANCODE_LEFT, LEFT); K(SDL_SCANCODE_RIGHT, RIGHT);
    K(SDL_SCANCODE_W, UP); K(SDL_SCANCODE_S, DOWN); K(SDL_SCANCODE_A, LEFT); K(SDL_SCANCODE_D, RIGHT);
    K(SDL_SCANCODE_RETURN, STA); K(SDL_SCANCODE_KP_ENTER, STA); K(SDL_SCANCODE_BACKSPACE, SEL);
    K(SDL_SCANCODE_K, CRO); K(SDL_SCANCODE_L, CIR); K(SDL_SCANCODE_J, SQU); K(SDL_SCANCODE_I, TRI);
    K(SDL_SCANCODE_SPACE, CIR);   // Esc opens the in-game menu (MENU.md), no longer CROSS
    K(SDL_SCANCODE_Q, L1); K(SDL_SCANCODE_E, R1); K(SDL_SCANCODE_1, L2); K(SDL_SCANCODE_3, R2);
    K(SDL_SCANCODE_Z, L3); K(SDL_SCANCODE_C, R3);
    K(SDL_SCANCODE_KP_8, RS_UP); K(SDL_SCANCODE_KP_5, RS_DOWN); K(SDL_SCANCODE_KP_4, RS_LEFT); K(SDL_SCANCODE_KP_6, RS_RIGHT);
    auto P = [](SDL_GamepadButton b, uint32_t m) { g_padmap[b] = m; };
    P(SDL_GAMEPAD_BUTTON_SOUTH, CRO); P(SDL_GAMEPAD_BUTTON_EAST, CIR);
    P(SDL_GAMEPAD_BUTTON_WEST, SQU); P(SDL_GAMEPAD_BUTTON_NORTH, TRI);
    P(SDL_GAMEPAD_BUTTON_BACK, SEL); P(SDL_GAMEPAD_BUTTON_START, STA);
    P(SDL_GAMEPAD_BUTTON_LEFT_STICK, L3); P(SDL_GAMEPAD_BUTTON_RIGHT_STICK, R3);
    P(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, L1); P(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, R1);
    P(SDL_GAMEPAD_BUTTON_DPAD_UP, UP); P(SDL_GAMEPAD_BUTTON_DPAD_DOWN, DOWN);
    P(SDL_GAMEPAD_BUTTON_DPAD_LEFT, LEFT); P(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, RIGHT);
}

std::map<SDL_Scancode, uint32_t> g_def_keymap;
std::map<int, uint32_t> g_def_padmap;

void ApplyIni() {
    g_deadzone = xsgcfg::GetInt("gamepad.deadzone");
    g_trigger = xsgcfg::GetInt("gamepad.trigger_threshold");
    for (auto& [k, v] : xsgcfg::Raw("keyboard")) {
        SDL_Scancode sc = SDL_GetScancodeFromName(k.c_str());
        uint32_t m;
        if (sc == SDL_SCANCODE_UNKNOWN) { Log("[keyboard] unknown key \"%s\"", k.c_str()); continue; }
        if (!ParseTargets(v, &m)) { Log("[keyboard] %s: unknown button \"%s\"", k.c_str(), v.c_str()); continue; }
        g_keymap[sc] = m;
    }
    for (auto& [k, v] : xsgcfg::Raw("gamepad")) {
        std::string lk = k;
        for (auto& c : lk) c = char(std::tolower((unsigned char)c));
        if (lk == "deadzone" || lk == "trigger_threshold" || lk == "layout") continue;   // cfg table keys
        uint32_t m;
        if (!ParseTargets(v, &m)) { Log("[gamepad] %s: unknown button \"%s\"", k.c_str(), v.c_str()); continue; }
        if (lk == "lefttrigger") { g_lt = m; continue; }
        if (lk == "righttrigger") { g_rt = m; continue; }
        SDL_GamepadButton b = ButtonFromName(lk);
        if (b == SDL_GAMEPAD_BUTTON_INVALID) { Log("[gamepad] unknown button \"%s\"", k.c_str()); continue; }
        g_padmap[b] = m;
    }
}

void Open(SDL_JoystickID id) {
    if (g_pads.count(id) && g_pads[id].gp) return;
    SDL_Gamepad* gp = SDL_OpenGamepad(id);
    if (!gp) { Log("cannot open gamepad %u: %s", unsigned(id), SDL_GetError()); return; }
    g_pads[id] = PadState{};
    g_pads[id].gp = gp;
    Log("gamepad %u connected: %s", unsigned(id), SDL_GetGamepadName(gp));
}

/* XSG_GPU_INPUT_TEST=FRAME:KIND:NAME[:VALUE]:HOLD,...  pushes synthetic SDL
   events at game vblank FRAME, and the release HOLD vblanks later.
   KIND key (NAME = SDL scancode name, e.g. W, Return), button (SDL gamepad
   button name: south, east, dpup...), axis (leftx, lefty, rightx, righty,
   lefttrigger, righttrigger; VALUE -32768..32767). */
struct TestEv { uint32_t frame, hold; int kind; int code; int value; };
std::vector<TestEv> g_test;
constexpr SDL_JoystickID kTestPad = 0x7e570000;

void ParseTest(const char* s) {
    std::string all = s;
    size_t i = 0;
    while (i < all.size()) {
        size_t j = all.find(',', i);
        if (j == std::string::npos) j = all.size();
        std::vector<std::string> f;
        std::string e = all.substr(i, j - i);
        for (size_t a = 0;;) {
            size_t b = e.find(':', a);
            f.push_back(Trim(e.substr(a, b == std::string::npos ? std::string::npos : b - a)));
            if (b == std::string::npos) break;
            a = b + 1;
        }
        i = j + 1;
        TestEv t = {};
        bool ok = f.size() >= 4;
        if (ok) {
            t.frame = uint32_t(std::strtoul(f[0].c_str(), nullptr, 0));
            t.hold = uint32_t(std::strtoul(f.back().c_str(), nullptr, 0));
            std::string kind = Upper(f[1]);
            if (kind == "KEY") {
                t.kind = 0;
                t.code = SDL_GetScancodeFromName(f[2].c_str());
                ok = t.code != SDL_SCANCODE_UNKNOWN;
            } else if (kind == "BUTTON") {
                t.kind = 1;
                t.code = ButtonFromName(f[2]);
                ok = t.code != SDL_GAMEPAD_BUTTON_INVALID;
            } else if (kind == "AXIS" && f.size() >= 5) {
                t.kind = 2;
                t.code = SDL_GetGamepadAxisFromString(f[2].c_str());
                t.value = std::atoi(f[3].c_str());
                ok = t.code != SDL_GAMEPAD_AXIS_INVALID;
            } else ok = false;
        }
        if (!ok) { Log("XSG_GPU_INPUT_TEST: bad entry \"%s\"", e.c_str()); continue; }
        g_test.push_back(t);
    }
    if (!g_test.empty()) {
        Log("XSG_GPU_INPUT_TEST: %zu synthetic events", g_test.size());
        g_pads[kTestPad] = PadState{};  // a virtual gamepad for button/axis entries
    }
}

void Push(const TestEv& t, bool down) {
    SDL_Event e;
    SDL_zero(e);
    if (t.kind == 0) {
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.scancode = SDL_Scancode(t.code);
        e.key.key = SDL_GetKeyFromScancode(SDL_Scancode(t.code), SDL_KMOD_NONE, false);   // for the menu (ImGui)
        e.key.down = down;
    } else if (t.kind == 1) {
        e.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
        e.gbutton.which = kTestPad;
        e.gbutton.button = Uint8(t.code);
        e.gbutton.down = down;
    } else {
        e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        e.gaxis.which = kTestPad;
        e.gaxis.axis = Uint8(t.code);
        e.gaxis.value = Sint16(down ? t.value : 0);
    }
    if (!SDL_PushEvent(&e)) Log("SDL_PushEvent: %s", SDL_GetError());
}

uint8_t AxisByte(int v) {
    if (v > -g_deadzone && v < g_deadzone) return 0x80;
    int b = (v + 32768) >> 8;
    return uint8_t(b < 0 ? 0 : b > 255 ? 255 : b);
}
#endif

}  // namespace

std::string Option(const char* key, const char* env, const char* def) {
    (void)env;
    (void)def;
    return xsgcfg::Get(("video." + std::string(key)).c_str());   // env > settings.ini > default (menu/cfg.cpp)
}

void Init(GpuPad* pad, bool sdl) {
    g_pad = pad;
    std::memset((void*)pad, 0, sizeof *pad);
#ifdef XSG_GPU_SDL
    g_sdl = sdl;
    DefaultMaps();   // also without SDL events: the menu shows and edits the bindings
    g_def_keymap = g_keymap;
    g_def_padmap = g_padmap;
    g_def_padmap[kLT] = L2;
    g_def_padmap[kRT] = R2;
    ApplyIni();
    if (!sdl) return;
    if (const char* t = std::getenv("XSG_GPU_INPUT_TEST"); t && *t) ParseTest(t);
    int n = 0;
    if (SDL_JoystickID* ids = SDL_GetGamepads(&n)) {
        for (int i = 0; i < n; ++i) Open(ids[i]);
        SDL_free(ids);
    }
    Log("ready: keyboard (%zu keys), %d gamepad(s), hotplug on", g_keymap.size(), n);
#else
    (void)sdl;
#endif
}

#ifdef XSG_GPU_SDL
bool Event(const SDL_Event& e) {
    if (!g_sdl) return false;
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (e.key.scancode < SDL_SCANCODE_COUNT) g_keys[e.key.scancode] = e.type == SDL_EVENT_KEY_DOWN;
        return true;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        std::memset(g_keys, 0, sizeof g_keys);  // no stuck keys after alt-tab
        return false;
    case SDL_EVENT_GAMEPAD_ADDED: Open(e.gdevice.which); return true;
    case SDL_EVENT_GAMEPAD_REMOVED: {
        auto it = g_pads.find(e.gdevice.which);
        if (it != g_pads.end()) {
            Log("gamepad %u disconnected", unsigned(e.gdevice.which));
            if (it->second.gp) SDL_CloseGamepad(it->second.gp);
            g_pads.erase(it);
        }
        return true;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        auto it = g_pads.find(e.gbutton.which);
        if (it != g_pads.end() && e.gbutton.button < SDL_GAMEPAD_BUTTON_COUNT)
            it->second.btn[e.gbutton.button] = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        auto it = g_pads.find(e.gaxis.which);
        if (it != g_pads.end() && e.gaxis.axis < SDL_GAMEPAD_AXIS_COUNT) it->second.axis[e.gaxis.axis] = e.gaxis.value;
        return true;
    }
    default: return false;
    }
}
#endif

void BeginFrame(uint32_t frame) {
#ifdef XSG_GPU_SDL
    if (!g_sdl) return;
    for (auto& t : g_test) {
        if (frame == t.frame) Push(t, true);
        if (frame == t.frame + t.hold) Push(t, false);
    }
#else
    (void)frame;
#endif
}

void PumpHeadless() {
#ifdef XSG_GPU_SDL
    if (!g_sdl) return;
    SDL_Event e;
    while (SDL_PollEvent(&e)) Event(e);
#endif
}

void Publish() {
    if (!g_pad) return;
#ifdef XSG_GPU_SDL
    if (!g_sdl) return;  // never active: the game keeps a neutral pad
    if (g_menu_open) {   // the in-game menu has the input: a neutral pad
        GpuPad* p = g_pad;
        const uint32_t s = p->seq;
        __atomic_store_n(&p->seq, s + 1, __ATOMIC_RELAXED);
        __atomic_thread_fence(__ATOMIC_RELEASE);
        p->buttons = 0;
        for (int i = 0; i < 4; ++i) p->stick[i] = 0x80;
        for (int i = 0; i < 12; ++i) p->press[i] = 0;
        p->updates = p->updates + 1;
        p->active = 1;
        __atomic_store_n(&p->seq, s + 2, __ATOMIC_RELEASE);
        return;
    }
    uint32_t m = 0;
    for (int s = 0; s < SDL_SCANCODE_COUNT; ++s)
        if (g_keys[s]) {
            auto it = g_keymap.find(SDL_Scancode(s));
            if (it != g_keymap.end()) m |= it->second;
        }
    int ax[SDL_GAMEPAD_AXIS_COUNT] = {};
    for (auto& [id, p] : g_pads) {
        for (auto& [b, mask] : g_padmap)
            if (p.btn[b]) m |= mask;
        for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a)
            if (std::abs(p.axis[a]) > std::abs(ax[a])) ax[a] = p.axis[a];
    }
    if (ax[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] > g_trigger) m |= g_lt;
    if (ax[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] > g_trigger) m |= g_rt;
    uint8_t st[4] = {AxisByte(ax[SDL_GAMEPAD_AXIS_RIGHTX]), AxisByte(ax[SDL_GAMEPAD_AXIS_RIGHTY]),
                     AxisByte(ax[SDL_GAMEPAD_AXIS_LEFTX]), AxisByte(ax[SDL_GAMEPAD_AXIS_LEFTY])};
    // Keyboard stick targets push the stick to its edge.
    if (m & LS_LEFT) st[2] = 0x00;
    if (m & LS_RIGHT) st[2] = 0xff;
    if (m & LS_UP) st[3] = 0x00;
    if (m & LS_DOWN) st[3] = 0xff;
    if (m & RS_LEFT) st[0] = 0x00;
    if (m & RS_RIGHT) st[0] = 0xff;
    if (m & RS_UP) st[1] = 0x00;
    if (m & RS_DOWN) st[1] = 0xff;
    const uint32_t buttons = m & 0xffff;
    uint8_t pr[12];
    for (int i = 0; i < 12; ++i) pr[i] = (buttons & kPressBit[i]) ? 0xff : 0;
    // Analog triggers give real L2/R2 pressure when mapped to them.
    auto trig = [](int v) { return uint8_t(v <= 0 ? 0 : v >= 32767 ? 255 : v >> 7); };
    if (g_lt == L2 && ax[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] > g_trigger) pr[10] = trig(ax[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]);
    if (g_rt == R2 && ax[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] > g_trigger) pr[11] = trig(ax[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]);
    uint32_t gamepads = 0;
    for (auto& [id, p] : g_pads) gamepads += p.gp != nullptr;

    GpuPad* p = g_pad;
    const uint32_t s = p->seq;
    __atomic_store_n(&p->seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    p->buttons = buttons;
    for (int i = 0; i < 4; ++i) p->stick[i] = st[i];
    for (int i = 0; i < 12; ++i) p->press[i] = pr[i];
    p->gamepads = gamepads;
    p->updates = p->updates + 1;
    p->active = 1;
    __atomic_store_n(&p->seq, s + 2, __ATOMIC_RELEASE);
#endif
}

/* ---------------------------------------------------------------- in-game menu */
void SetMenuOpen(bool open) {
    g_menu_open = open;
#ifdef XSG_GPU_SDL
    std::memset(g_keys, 0, sizeof g_keys);   // no key held across the menu
#endif
}

bool MenuOpen() { return g_menu_open; }

uint32_t TargetMask(const char* name) {
    uint32_t m = 0;
    return ParseTargets(name, &m) ? m : 0;
}

std::string TargetNames(uint32_t m) {
    if (!m) return "NONE";
    std::string r;
    for (auto& k : kTargets) {
        if (!k.mask || !std::strcmp(k.name, "X") || !std::strcmp(k.name, "O")) continue;
        if ((m & k.mask) == k.mask) { r += r.empty() ? "" : "+"; r += k.name; m &= ~k.mask; }
    }
    return r;
}

void SetDeadzone(int dz, int trig) {
    g_deadzone = dz;
    g_trigger = trig;
}

#ifdef XSG_GPU_SDL
bool MenuPad(uint32_t* buttons, int* lx, int* ly) {
    uint32_t b = 0;
    int x = 0, y = 0;
    bool any = false;
    for (auto& [id, p] : g_pads) {
        any = any || p.gp != nullptr || id == kTestPad;
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT && i < 32; ++i)
            if (p.btn[i]) b |= 1u << i;
        if (std::abs(p.axis[SDL_GAMEPAD_AXIS_LEFTX]) > std::abs(x)) x = p.axis[SDL_GAMEPAD_AXIS_LEFTX];
        if (std::abs(p.axis[SDL_GAMEPAD_AXIS_LEFTY]) > std::abs(y)) y = p.axis[SDL_GAMEPAD_AXIS_LEFTY];
    }
    *buttons = b;
    *lx = x;
    *ly = y;
    return any;
}

std::vector<std::pair<int, uint32_t>> KeyBinds() { return {g_keymap.begin(), g_keymap.end()}; }
std::vector<std::pair<int, uint32_t>> PadBinds() {
    std::vector<std::pair<int, uint32_t>> v(g_padmap.begin(), g_padmap.end());
    v.push_back({kLT, g_lt});
    v.push_back({kRT, g_rt});
    return v;
}
void SetKeyBind(int scancode, uint32_t mask) {
    if (mask) g_keymap[SDL_Scancode(scancode)] = mask;
    else g_keymap.erase(SDL_Scancode(scancode));
}
void SetPadBind(int button, uint32_t mask) {
    if (button == kLT) g_lt = mask;
    else if (button == kRT) g_rt = mask;
    else if (mask) g_padmap[button] = mask;
    else g_padmap.erase(button);
}
const char* PadButtonName(int b) {
    if (b == kLT) return "lefttrigger";
    if (b == kRT) return "righttrigger";
    static const char* kPos[] = {"south", "east", "west", "north"};
    if (b >= 0 && b < 4) return kPos[b];
    const char* n = SDL_GetGamepadStringForButton(SDL_GamepadButton(b));
    return n ? n : "?";
}

/* The remapping as settings.ini entries: only what differs from the defaults
   (NONE for an unbound default), so the defaults can change later. */
void SaveBinds() {
    auto& kb = xsgcfg::Raw("keyboard");
    kb.clear();
    for (auto& [sc, m] : g_def_keymap)
        if (!g_keymap.count(sc)) kb[SDL_GetScancodeName(sc)] = "NONE";
    for (auto& [sc, m] : g_keymap) {
        auto d = g_def_keymap.find(sc);
        if (d == g_def_keymap.end() || d->second != m) kb[SDL_GetScancodeName(sc)] = TargetNames(m);
    }
    auto& gp = xsgcfg::Raw("gamepad");
    for (auto it = gp.begin(); it != gp.end();) {   // keep the cfg table keys (deadzone, layout...)
        std::string l = it->first;
        for (auto& c : l) c = char(std::tolower((unsigned char)c));
        it = (l == "deadzone" || l == "trigger_threshold" || l == "layout") ? std::next(it) : gp.erase(it);
    }
    auto cur = g_padmap;
    cur[kLT] = g_lt;
    cur[kRT] = g_rt;
    for (auto& [b, m] : g_def_padmap)
        if (!cur.count(b) || (cur[b] == 0 && m)) gp[PadButtonName(b)] = "NONE";
    for (auto& [b, m] : cur) {
        auto d = g_def_padmap.find(b);
        if (m && (d == g_def_padmap.end() || d->second != m)) gp[PadButtonName(b)] = TargetNames(m);
    }
    xsgcfg::RawChanged();
}

void ResetBinds() {
    g_keymap = g_def_keymap;
    g_padmap = g_def_padmap;
    g_padmap.erase(kLT);
    g_padmap.erase(kRT);
    g_lt = L2;
    g_rt = R2;
    SaveBinds();
}
#endif

}  // namespace xsginput

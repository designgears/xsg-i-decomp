/*
 * Settings table and ~/.config/xsg/settings.ini (port/docs/MENU.md).
 * Structure after TotK Recomp's runtime/cfg.h (MIT, Copyright (c) 2026
 * Valiantsin Dzerakh): a meta table {key, env, default, values, live},
 * env > ini > default, a debounced save.  Our own implementation.
 */
#include "cfg.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace xsgcfg {
namespace {

const Meta kTable[] = {
    // Display (live)
    {"video.fullscreen", "XSG_GPU_FULLSCREEN", "0", "0|1", true},
    {"video.window_scale", "XSG_GPU_SCALE", "2", "1..4", true},
    {"video.present", "XSG_GPU_PRESENT", "mailbox", "mailbox|fifo|immediate|auto", true},
    {"video.aspect", "XSG_GPU_ASPECT", "4:3", "4:3|16:9|stretch", true},
    {"video.scale_mode", "XSG_GPU_SCALEMODE", "fit", "fit|integer", true},
    {"video.filter", "XSG_GPU_FILTER", "linear", "linear|nearest", true},
    // Renderer (after restart; exported to the environment by the game, gpu_client.cpp)
    {"renderer.native", "XSG_GPU_NATIVE", "0", "0|1", false},
    {"renderer.native_scale", "XSG_GPU_NATIVE_SCALE", "2", "1..8", false},
    {"renderer.ssaa", "XSG_GPU_SSAA", "1", "1|2|4|8|16", false},
    {"renderer.ssaa_textures", "XSG_GPU_SSAA_TEX", "0", "0|1", false},
    {"renderer.hires_scanout", "XSG_GPU_HIRES", "0", "0|1", false},
    // Game (live, applied by the game process through GpuSettings)
    {"game.fps_limit", "XSG_FRAME_US", "60", "60|30|120|uncapped", true},
    {"audio.enabled", "XSG_AUDIO", "1", "0|1", true},
    {"audio.master", nullptr, "100", "0..100", true},
    {"audio.music", nullptr, "100", "0..100", true},
    {"audio.effects", nullptr, "100", "0..100", true},
    {"audio.voice", nullptr, "100", "0..100", true},
    // Controls
    {"gamepad.deadzone", nullptr, "8000", "0..32000", true},
    {"gamepad.trigger_threshold", nullptr, "8000", "0..32000", true},
    {"gamepad.layout", nullptr, "xbox", "xbox|playstation|nintendo", true},
    // Info
    {"overlay.mode", "XSG_GPU_OVERLAY", "off", "off|fps|full", true},
};
constexpr int kN = int(sizeof kTable / sizeof kTable[0]);

// section (lower case) -> key (as written) -> value
std::map<std::string, std::map<std::string, std::string>> g_ini;
std::vector<std::string> g_order;   // section order for writing
unsigned g_version = 1;
bool g_dirty, g_save_ok = true;
std::chrono::steady_clock::time_point g_dirty_at;
std::string g_path;
std::string g_from_ini;   // XSG_CFG_FROM_INI: env vars the game exported from settings.ini

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[xsg-gpu] cfg: %s\n", b);
}

std::string Lower(std::string s) {
    for (auto& c : s) c = char(std::tolower((unsigned char)c));
    return s;
}
std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
void Split(const char* key, std::string* sec, std::string* name) {
    const char* d = std::strchr(key, '.');
    *sec = d ? std::string(key, d - key) : std::string();
    *name = d ? d + 1 : key;
}
std::map<std::string, std::string>& Sec(const std::string& s) {
    std::string l = Lower(s);
    if (!g_ini.count(l)) g_order.push_back(l);
    return g_ini[l];
}
/* Case-insensitive lookup in a section. */
const std::string* Lookup(const std::string& sec, const std::string& name) {
    auto it = g_ini.find(Lower(sec));
    if (it == g_ini.end()) return nullptr;
    for (auto& [k, v] : it->second)
        if (Lower(k) == Lower(name)) return &v;
    return nullptr;
}

int ReadFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return -1;
    char line[1024];
    std::string sec;
    int n = 0;
    while (std::fgets(line, sizeof line, f)) {
        std::string l = Trim(line);
        if (l.empty() || l[0] == '#' || l[0] == ';') continue;
        if (l[0] == '[') { sec = Lower(Trim(l.substr(1, l.find(']') - 1))); Sec(sec); continue; }
        size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(l.substr(0, eq)), v = Trim(l.substr(eq + 1));
        if (size_t h = v.find(" #"); h != std::string::npos) v = Trim(v.substr(0, h));   // trailing comment
        auto& s = Sec(sec);
        for (auto it = s.begin(); it != s.end();)   // one entry per key, any case
            it = Lower(it->first) == Lower(k) ? s.erase(it) : std::next(it);
        s[k] = v;
        n++;
    }
    std::fclose(f);
    return n;
}

std::string ConfigDir() {
    if (const char* x = std::getenv("XDG_CONFIG_HOME"); x && *x) return std::string(x) + "/xsg";
    if (const char* h = std::getenv("HOME"); h && *h) return std::string(h) + "/.config/xsg";
    return ".";
}

bool EnvSet(const char* env) {
    if (!env) return false;
    const char* v = std::getenv(env);
    if (!v || !*v) return false;
    // exported by the game from settings.ini, not by the user
    std::string l = "," + g_from_ini + ",";
    return l.find("," + std::string(env) + ",") == std::string::npos;
}

}  // namespace

void Load() {
    if (const char* f = std::getenv("XSG_CFG_FROM_INI")) g_from_ini = f;
    if (const char* p = std::getenv("XSG_SETTINGS_INI"); p && *p) g_path = p;
    else g_path = ConfigDir() + "/settings.ini";
    std::string legacy = ConfigDir() + "/input.ini";
    const char* explicit_input = std::getenv("XSG_INPUT_INI");
    int n = ReadFile(g_path);
    if (n >= 0) {
        Log("%s: %d entries", g_path.c_str(), n);
    } else {
        // First run with the menu: migrate input.ini (kept as it is) into settings.ini.
        int m = ReadFile(legacy);
        if (m >= 0) {
            Log("migrating %s (%d entries) into %s", legacy.c_str(), m, g_path.c_str());
            g_dirty = true;
            g_dirty_at = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        }
    }
    if (explicit_input && *explicit_input) {   // backward compatibility: an explicit input.ini wins
        int m = ReadFile(explicit_input);
        Log("XSG_INPUT_INI %s: %d entries", explicit_input, m);
    }
    for (int i = 0; i < kN; ++i)
        if (EnvSet(kTable[i].env)) Log("%s set by %s=%s (greyed out in the menu)", kTable[i].key, kTable[i].env, std::getenv(kTable[i].env));
}

const Meta* Find(const char* key) {
    for (auto& m : kTable)
        if (!std::strcmp(m.key, key)) return &m;
    return nullptr;
}
const Meta* At(int i) { return i >= 0 && i < kN ? &kTable[i] : nullptr; }

bool FromEnv(const char* key) {
    const Meta* m = Find(key);
    if (!m) return false;
    if (!std::strcmp(key, "game.fps_limit") && EnvSet("XSG_FAST")) return true;
    if (!std::strcmp(key, "video.present") && EnvSet("XSG_GPU_VSYNC") && !EnvSet("XSG_GPU_PRESENT")) return true;
    return EnvSet(m->env);
}

std::string EnvName(const char* key) {
    const Meta* m = Find(key);
    if (!m) return "";
    if (!std::strcmp(key, "game.fps_limit") && EnvSet("XSG_FAST")) return "XSG_FAST";
    if (!std::strcmp(key, "video.present") && EnvSet("XSG_GPU_VSYNC") && !EnvSet("XSG_GPU_PRESENT")) return "XSG_GPU_VSYNC";
    return m->env && EnvSet(m->env) ? m->env : "";
}

std::string Get(const char* key) {
    const Meta* m = Find(key);
    if (m && m->env && FromEnv(key)) {
        if (!std::strcmp(key, "game.fps_limit")) {   // XSG_FAST / XSG_FRAME_US -> the row's value
            if (EnvSet("XSG_FAST") && std::strcmp(std::getenv("XSG_FAST"), "0")) return "uncapped";
            long us = std::atol(std::getenv("XSG_FRAME_US") ? std::getenv("XSG_FRAME_US") : "16667");
            return us == 0 ? "uncapped" : std::to_string(int(1e6 / us + 0.5));
        }
        if (!std::strcmp(key, "video.present") && !EnvSet("XSG_GPU_PRESENT")) return "fifo";
        return std::getenv(m->env);
    }
    std::string sec, name;
    Split(key, &sec, &name);
    if (const std::string* v = Lookup(sec, name); v && !v->empty()) return *v;
    if (m && m->env) {   // exported by the game from settings.ini (same value) or a legacy env
        if (const char* v = std::getenv(m->env); v && *v) return v;
    }
    return m ? m->def : "";
}

int GetInt(const char* key) {
    std::string v = Get(key);
    char* end = nullptr;
    long r = std::strtol(v.c_str(), &end, 0);
    if (end == v.c_str()) {
        const Meta* m = Find(key);
        return m ? std::atoi(m->def) : 0;
    }
    return int(r);
}

bool GetBool(const char* key) {
    std::string v = Get(key);
    return !v.empty() && v != "0" && v != "off" && v != "false";
}

void Set(const char* key, const std::string& value) {
    std::string sec, name;
    Split(key, &sec, &name);
    auto& s = Sec(sec);
    for (auto it = s.begin(); it != s.end();)
        it = Lower(it->first) == Lower(name) ? s.erase(it) : std::next(it);
    s[name] = value;
    g_version++;
    g_dirty = true;
    g_dirty_at = std::chrono::steady_clock::now();
}

unsigned Version() { return g_version; }

std::map<std::string, std::string>& Raw(const char* section) { return Sec(section); }

void RawChanged() {
    g_version++;
    g_dirty = true;
    g_dirty_at = std::chrono::steady_clock::now();
}

std::string Path() { return g_path; }
bool LastSaveOk() { return g_save_ok; }

bool SaveNow() {
    g_dirty = false;
    std::string dir = g_path.substr(0, g_path.rfind('/'));
    if (!dir.empty() && dir != g_path) {
        std::string p;
        for (size_t i = 1; i <= dir.size(); ++i)
            if (i == dir.size() || dir[i] == '/') ::mkdir(dir.substr(0, i).c_str(), 0755);
    }
    std::string tmp = g_path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) {
        Log("cannot write %s", tmp.c_str());
        g_save_ok = false;
        return false;
    }
    std::fprintf(f, "# Xenosaga Episode I PC port settings (written by the in-game menu, F1).\n"
                    "# Environment variables (XSG_*) override these.  See port/docs/MENU.md.\n");
    static const char* kOrder[] = {"video", "renderer", "game", "audio", "overlay", "keyboard", "gamepad"};
    std::vector<std::string> order(std::begin(kOrder), std::end(kOrder));
    for (auto& s : g_order)
        if (std::find(order.begin(), order.end(), s) == order.end()) order.push_back(s);
    for (auto& s : order) {
        auto it = g_ini.find(s);
        if (it == g_ini.end() || it->second.empty()) continue;
        std::fprintf(f, "\n[%s]\n", s.c_str());
        for (auto& [k, v] : it->second) std::fprintf(f, "%s = %s\n", k.c_str(), v.c_str());
    }
    bool ok = std::fclose(f) == 0 && std::rename(tmp.c_str(), g_path.c_str()) == 0;
    g_save_ok = ok;
    Log("%s %s", ok ? "saved" : "FAILED to save", g_path.c_str());
    return ok;
}

bool SaveTick(bool force) {
    if (!g_dirty) return false;
    if (force || std::chrono::steady_clock::now() - g_dirty_at > std::chrono::milliseconds(600)) SaveNow();
    return g_dirty;
}

}  // namespace xsgcfg

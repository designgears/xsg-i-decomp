/*
 * Settings table and ~/.config/xsg/settings.ini (port/docs/MENU.md).
 * Structure after TotK Recomp's runtime/cfg.h (MIT, Copyright (c) 2026
 * Valiantsin Dzerakh): a meta table {key, default, values, live}, ini >
 * default, a debounced save.  Our own implementation.  The table itself is
 * port/runtime/xsg_settings_table.h, shared with the game process.
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

extern char** environ;

namespace xsgcfg {
namespace {

const Meta kTable[] = {
#define XSG_SETTING(key, env, def, values, live, desc) {key, env, def, values, live != 0, desc},
#include "xsg_settings_table.h"
#undef XSG_SETTING
};
constexpr int kN = int(sizeof kTable / sizeof kTable[0]);

// section (lower case) -> key (as written) -> value
std::map<std::string, std::map<std::string, std::string>> g_ini;
std::vector<std::string> g_order;   // section order for writing
unsigned g_version = 1;
bool g_dirty, g_save_ok = true;
std::chrono::steady_clock::time_point g_dirty_at;
std::string g_path;
std::vector<std::string> g_ignored_env;
std::map<std::string, std::string> g_cstr;   // CStr() storage

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

}  // namespace

void Load() {
    if (const char* p = std::getenv("XSG_CONFIG"); p && *p) g_path = p;   // the one environment variable
    else g_path = ConfigDir() + "/settings.ini";
    std::string legacy = ConfigDir() + "/input.ini";
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
    // [video] was renamed [display]: move its entries (an explicit [display] value wins).
    if (auto it = g_ini.find("video"); it != g_ini.end()) {
        auto old = it->second;
        g_ini.erase(it);
        g_order.erase(std::remove(g_order.begin(), g_order.end(), std::string("video")), g_order.end());
        auto& d = Sec("display");
        for (auto& [k, v] : old)
            if (!Lookup("display", k)) d[k] = v;
        Log("moved [video] to [display]");
        g_dirty = true;
        g_dirty_at = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    }
    // The environment no longer sets anything; the game logs the deprecation (settings.c).
    for (char** e = environ; *e; ++e)
        if (!std::strncmp(*e, "XSG_", 4) && std::strncmp(*e, "XSG_CONFIG=", 11)) g_ignored_env.push_back(*e);
    std::sort(g_ignored_env.begin(), g_ignored_env.end());
}

const std::vector<std::string>& IgnoredEnv() { return g_ignored_env; }

const Meta* Find(const char* key) {
    for (auto& m : kTable)
        if (!std::strcmp(m.key, key)) return &m;
    return nullptr;
}
const Meta* At(int i) { return i >= 0 && i < kN ? &kTable[i] : nullptr; }

std::string Get(const char* key) {
    const Meta* m = Find(key);
    std::string sec, name;
    Split(key, &sec, &name);
    if (const std::string* v = Lookup(sec, name); v && !v->empty()) return *v;
    return m ? m->def : "";
}

const char* CStr(const char* key) {
    std::string v = Get(key);
    if (v.empty()) return nullptr;
    auto& slot = g_cstr[key];
    if (slot != v) slot = v;   // callers read it at start; a later change re-points
    return slot.c_str();
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
                    "# The only environment variable is XSG_CONFIG (another settings file).  See port/docs/MENU.md.\n");
    static const char* kOrder[] = {"display", "renderer", "game", "audio", "gamepad", "overlay", "keyboard", "debug"};
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

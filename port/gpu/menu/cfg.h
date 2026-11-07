/*
 * Settings of the PC port (port/docs/MENU.md): one table of {key, legacy env,
 * default, values, live, description} -- port/runtime/xsg_settings_table.h,
 * shared with the game process (port/runtime/settings.c) -- read from
 * ~/.config/xsg/settings.ini, or the file named by XSG_CONFIG, with the
 * precedence settings.ini > (legacy input.ini) > default.  No other
 * environment variable sets anything.
 *
 * Modelled on TotK Recomp's runtime/cfg.h (MIT, see THIRD_PARTY.md): the same
 * meta table, env check and debounced save, rewritten for an ini with
 * sections and for xsg-gpu (C++, one thread).
 *
 * Keys are "section.name" (video.aspect).  The [keyboard] and [gamepad]
 * sections also hold free-form remapping entries (STAGE10_INPUT.md §3),
 * reached through Raw().
 */
#ifndef XSG_MENU_CFG_H
#define XSG_MENU_CFG_H

#include <map>
#include <vector>
#include <string>

namespace xsgcfg {

struct Meta {
    const char* key;      // "video.aspect"
    const char* env;      // the legacy XSG_* variable it replaced (ignored now), or nullptr
    const char* def;      // default value
    const char* values;   // "a|b|c", a range "lo..hi", or nullptr (free text)
    bool live;            // applies at once; else after a restart
    const char* desc;     // one line (docs, the Debug tab)
};

/* Loads input.ini (legacy, STAGE10_INPUT.md) and then settings.ini over it. */
void Load();
const Meta* Find(const char* key);
const Meta* At(int i);   // table order; nullptr past the end

/* The value in effect: ini > default. */
std::string Get(const char* key);
int GetInt(const char* key);
bool GetBool(const char* key);
/* Get() as a C string, nullptr when empty (getenv() semantics for the
   start-up switches); the pointer stays valid. */
const char* CStr(const char* key);
/* Sets the ini value; marks it for saving. */
void Set(const char* key, const std::string& value);
/* Legacy XSG_* environment variables present at start (ignored; for Info). */
const std::vector<std::string>& IgnoredEnv();
/* Bumped by every change. */
unsigned Version();

/* Raw entries of a section ([keyboard], [gamepad]), keys as written. */
std::map<std::string, std::string>& Raw(const char* section);
void RawChanged();   // a Raw section was edited: save it

/* Debounced save: call every frame; writes ~0.6 s after the last change, or
   now with force.  Returns true while a write is pending. */
bool SaveTick(bool force);
bool SaveNow();
std::string Path();
bool LastSaveOk();

}  // namespace xsgcfg

#endif

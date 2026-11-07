/*
 * Settings of the PC port (port/docs/MENU.md): one table of {key, env,
 * default, values, live}, read from ~/.config/xsg/settings.ini with the
 * precedence environment > settings.ini > (legacy input.ini) > default.
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
#include <string>

namespace xsgcfg {

struct Meta {
    const char* key;      // "video.aspect"
    const char* env;      // XSG_GPU_ASPECT, or nullptr
    const char* def;      // default value
    const char* values;   // "a|b|c", a range "lo..hi", or nullptr (free text)
    bool live;            // applies at once; else after a restart
};

/* Loads input.ini (legacy, STAGE10_INPUT.md) and then settings.ini over it. */
void Load();
const Meta* Find(const char* key);
const Meta* At(int i);   // table order; nullptr past the end

/* The value in effect: env > ini > default. */
std::string Get(const char* key);
int GetInt(const char* key);
bool GetBool(const char* key);
/* Sets the ini value (an env override still wins); marks it for saving. */
void Set(const char* key, const std::string& value);
/* The environment fixes this key (greyed out in the menu). */
bool FromEnv(const char* key);
/* The env var name that fixes it ("" when none). */
std::string EnvName(const char* key);
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

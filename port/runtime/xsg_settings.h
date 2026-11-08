/*
 * settings.ini for the game process (port/runtime/settings.c).
 *
 * The file is ~/.config/xsg/settings.ini ($XDG_CONFIG_HOME/xsg/settings.ini),
 * or the path in XSG_CONFIG, the one environment variable the port still
 * reads.  Keys are "section.name" from xsg_settings_table.h, shared with
 * xsg-gpu's menu (port/gpu/menu/cfg.cpp).  The file is read at the first
 * call and again whenever it changes (xsg_cfg_poll, every vblank), so the menu's
 * saves reach the game; the FPS limit and volumes also come through GpuSettings
 * (gpu_ipc.h).
 */
#ifndef XSG_SETTINGS_H
#define XSG_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* The value in effect (settings.ini, else the table default), or NULL when
   that is empty -- like getenv() of an unset variable. */
const char *xsg_cfg(const char *key);
/* Re-read settings.ini if it changed since (xsg-gpu's menu saves it); 1 if it did.
 * xsg_cfg_gen counts the reloads, for code that caches a setting. */
int xsg_cfg_poll(void);
extern volatile unsigned xsg_cfg_gen;
/* Integer value, or def when unset / not a number. */
long xsg_cfg_int(const char *key, long def);
/* Set and not "0" / "off" / "false" / "no". */
int xsg_cfg_on(const char *key);
/* The settings file in use. */
const char *xsg_cfg_path(void);
/* Logs the file and warns about legacy XSG_* environment variables, which
   are ignored (called once logging is up). */
void xsg_cfg_report(void);

#ifdef __cplusplus
}
#endif

#endif

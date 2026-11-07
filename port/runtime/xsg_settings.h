/*
 * settings.ini for the game process (port/runtime/settings.c).
 *
 * The file is ~/.config/xsg/settings.ini ($XDG_CONFIG_HOME/xsg/settings.ini),
 * or the path in XSG_CONFIG, the one environment variable the port still
 * reads.  Keys are "section.name" from xsg_settings_table.h, shared with
 * xsg-gpu's menu (port/gpu/menu/cfg.cpp).  The file is read once, at the
 * first call; settings that change while the game runs come from xsg-gpu
 * through GpuSettings (gpu_ipc.h).
 */
#ifndef XSG_SETTINGS_H
#define XSG_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* The value in effect (settings.ini, else the table default), or NULL when
   that is empty -- like getenv() of an unset variable. */
const char *xsg_cfg(const char *key);
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

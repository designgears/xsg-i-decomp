/*
 * settings.ini reader for the game process (xsg_settings.h,
 * port/docs/MENU.md).  A small ini parser over the shared schema
 * (xsg_settings_table.h); xsg-gpu writes the same file from its menu.
 */
#include "xsg_settings.h"
#include "xsg_rt.h"

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern char **environ;

typedef struct {
    const char *key, *env, *def;
} Entry;

static const Entry kTable[] = {
#define XSG_SETTING(key, env, def, values, live, desc) {key, env, def},
#include "xsg_settings_table.h"
#undef XSG_SETTING
};
#define N_ENTRIES ((int)(sizeof kTable / sizeof kTable[0]))

static char *g_val[N_ENTRIES];   /* from the file, NULL: not in it */
static char g_path[1024];
static int g_read = -1;          /* entries read, -1: no file */
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static int find(const char *key)
{
    for (int i = 0; i < N_ENTRIES; i++)
        if (!strcasecmp(kTable[i].key, key))
            return i;
    return -1;
}

static void load(void)
{
    const char *p = getenv("XSG_CONFIG");
    const char *x = getenv("XDG_CONFIG_HOME"), *h = getenv("HOME");
    if (p && *p)
        snprintf(g_path, sizeof g_path, "%s", p);
    else if (x && *x)
        snprintf(g_path, sizeof g_path, "%s/xsg/settings.ini", x);
    else
        snprintf(g_path, sizeof g_path, "%s/.config/xsg/settings.ini", h && *h ? h : ".");
    FILE *f = fopen(g_path, "r");
    if (!f)
        return;
    g_read = 0;
    char line[2048], sec[64] = "";
    while (fgets(line, sizeof line, f)) {
        char *l = trim(line);
        if (!*l || *l == '#' || *l == ';')
            continue;
        if (*l == '[') {
            char *e = strchr(l, ']');
            if (e)
                *e = 0;
            snprintf(sec, sizeof sec, "%s", trim(l + 1));
            continue;
        }
        char *eq = strchr(l, '=');
        if (!eq)
            continue;
        *eq = 0;
        char *k = trim(l), *v = trim(eq + 1);
        char *c = strstr(v, " #");   /* trailing comment */
        if (c) {
            *c = 0;
            v = trim(v);
        }
        char full[192];
        snprintf(full, sizeof full, "%s.%s", sec, k);
        int i = find(full);
        if (i < 0)
            continue;   /* bindings ([keyboard], [gamepad]) and unknown keys: xsg-gpu's */
        free(g_val[i]);
        g_val[i] = strdup(v);
        g_read++;
    }
    fclose(f);
}

const char *xsg_cfg(const char *key)
{
    pthread_once(&g_once, load);
    int i = find(key);
    if (i < 0) {
        xsg_logf("[settings] unknown key %s (not in xsg_settings_table.h)\n", key);
        return NULL;
    }
    const char *v = g_val[i] ? g_val[i] : kTable[i].def;
    return v && *v ? v : NULL;
}

long xsg_cfg_int(const char *key, long def)
{
    const char *v = xsg_cfg(key);
    if (!v)
        return def;
    char *e;
    long r = strtol(v, &e, 0);
    return e == v ? def : r;
}

int xsg_cfg_on(const char *key)
{
    const char *v = xsg_cfg(key);
    return v && strcmp(v, "0") && strcasecmp(v, "off") && strcasecmp(v, "false") && strcasecmp(v, "no");
}

const char *xsg_cfg_path(void)
{
    pthread_once(&g_once, load);
    return g_path;
}

void xsg_cfg_report(void)
{
    pthread_once(&g_once, load);
    if (g_read < 0)
        xsg_logf("[settings] %s: not found, using defaults\n", g_path);
    else
        xsg_logf("[settings] %s: %d settings\n", g_path, g_read);
    /* The environment no longer sets anything (except XSG_CONFIG). */
    for (char **e = environ; *e; e++) {
        if (strncmp(*e, "XSG_", 4) || !strncmp(*e, "XSG_CONFIG=", 11))
            continue;
        const char *eq = strchr(*e, '=');
        size_t n = eq ? (size_t)(eq - *e) : strlen(*e);
        int hit = -1;
        for (int i = 0; i < N_ENTRIES && hit < 0; i++)
            if (kTable[i].env && strlen(kTable[i].env) == n && !strncmp(kTable[i].env, *e, n))
                hit = i;
        if (hit >= 0) {
            const char *dot = strchr(kTable[hit].key, '.');
            const char *val = eq ? eq + 1 : "";
            if (!strcmp(kTable[hit].env, "XSG_FAST"))
                val = strcmp(val, "0") ? "uncapped" : "60";
            xsg_logf("[settings] DEPRECATED: %.*s is ignored; set [%.*s] %s = %s in %s\n", (int)n, *e,
                     (int)(dot - kTable[hit].key), kTable[hit].key, dot + 1, val, g_path);
        } else {
            xsg_logf("[settings] %.*s is ignored (settings come from %s only)\n", (int)n, *e, g_path);
        }
    }
}

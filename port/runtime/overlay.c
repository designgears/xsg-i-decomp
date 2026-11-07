/*
 * Overlays on the host.  All five overlays are linked into the executable;
 * "loading" one (sceSifLoadElf("cdrom0:\\OVxx.OVL;1")) makes it the active
 * overlay and restores its .data/.bss from a pristine copy taken at startup:
 * both the converted data (sections xsgd_<ov> / xsgb_<ov>, gen_host_data.py)
 * and the overlay's compiled-C statics (xsgcd_<ov> / xsgcb_<ov>, renamed from
 * .data/.bss by link_xsg.py).
 *
 * PS2 code reaches overlay functions through fixed addresses (func_A19xxx,
 * selection codes returned by the title screen...).  xsg_ps2_func() maps a PS2
 * address to the host function using the per-overlay tables link_xsg.py
 * generates from config/symbols/<ov>.txt (xsg_ovtab_<ov>), preferring the
 * active overlay.
 */
#include <stdlib.h>
#include <string.h>

#include "xsg_rt.h"

struct ovtab_ent {
    uint32_t addr;
    void *fn;
};

#define SEC(sec, name)                                                               \
    extern char __start_##sec##_##name[] __attribute__((weak));                       \
    extern char __stop_##sec##_##name[] __attribute__((weak));
#define OV(name)                                                                     \
    extern const struct ovtab_ent xsg_ovtab_##name[] __attribute__((weak));          \
    SEC(xsgd, name) SEC(xsgb, name) SEC(xsgcd, name) SEC(xsgcb, name)
OV(ov01) OV(ov02) OV(ov10) OV(ov11) OV(ov12)

/* [0] the converted data (xsgd_/xsgb_), [1] the overlay's compiled-C
 * .data/.bss, renamed to xsgcd_/xsgcb_ by link_xsg.py */
struct overlay {
    int id;
    const char *name;
    const struct ovtab_ent *tab;
    char *d0[2], *d1[2], *b0[2], *b1[2];
    char *pristine[2];
};
#define OVE(n, name)                                                                  \
    {n, #name, xsg_ovtab_##name,                                                     \
     {__start_xsgd_##name, __start_xsgcd_##name}, {__stop_xsgd_##name, __stop_xsgcd_##name}, \
     {__start_xsgb_##name, __start_xsgcb_##name}, {__stop_xsgb_##name, __stop_xsgcb_##name}, \
     {NULL, NULL}}
static struct overlay ovl[] = {OVE(1, ov01), OVE(2, ov02), OVE(10, ov10), OVE(11, ov11),
                               OVE(12, ov12)};
#define NOV (int)(sizeof ovl / sizeof ovl[0])

/* ov02 is part of the main ELF image (it sits at 0xA00000 from boot) and
 * xglCdInitial sets loaded_overlay = 2, so it is active from the start. */
int xsg_active_overlay = 2;

static size_t span(char *a, char *b) { return a && b ? (size_t)(b - a) : 0; }

void xsg_overlay_init(void)
{
    for (int i = 0; i < NOV; i++) {
        struct overlay *o = &ovl[i];
        int fns = 0;
        for (int k = 0; k < 2; k++) {
            size_t n = span(o->d0[k], o->d1[k]);
            if (n) {
                o->pristine[k] = malloc(n);
                memcpy(o->pristine[k], o->d0[k], n);
            }
        }
        for (const struct ovtab_ent *e = o->tab; e && e->addr; e++)
            fns++;
        xsg_logf("[ovl] %s: data %zu+%zu bytes, bss %zu+%zu bytes (converted+C), %d PS2-address "
                 "functions\n", o->name, span(o->d0[0], o->d1[0]), span(o->d0[1], o->d1[1]),
                 span(o->b0[0], o->b1[0]), span(o->b0[1], o->b1[1]), fns);
    }
}

/* Activating an overlay = the PS2 loading its image afresh: every initialised
 * static is back to its link-time value and .bss is zero. */
int xsg_overlay_activate(int id)
{
    for (int i = 0; i < NOV; i++) {
        struct overlay *o = &ovl[i];
        if (o->id != id)
            continue;
        for (int k = 0; k < 2; k++) {
            if (o->pristine[k])
                memcpy(o->d0[k], o->pristine[k], span(o->d0[k], o->d1[k]));
            if (span(o->b0[k], o->b1[k]))
                memset(o->b0[k], 0, span(o->b0[k], o->b1[k]));
        }
        xsg_logf("[ovl] overlay %s active (was %d)\n", o->name, xsg_active_overlay);
        xsg_active_overlay = id;
        return 0;
    }
    xsg_logf("[ovl] unknown overlay %d\n", id);
    return -1;
}

static void *lookup(const struct overlay *o, uint32_t addr)
{
    for (const struct ovtab_ent *e = o->tab; e && e->addr; e++)
        if (e->addr == addr)
            return e->fn;
    return NULL;
}

static void unresolved(void)
{
    xsg_logf("[ovl] called an unresolved PS2 overlay address\n");
    abort();
}

void *xsg_ps2_func(uint32_t addr)
{
    for (int i = 0; i < NOV; i++)
        if (ovl[i].id == xsg_active_overlay) {
            void *f = lookup(&ovl[i], addr);
            if (f)
                return f;
        }
    for (int i = 0; i < NOV; i++) {
        void *f = lookup(&ovl[i], addr);
        if (f) {
            STUB_LOG("0x%08x resolved in inactive overlay %s (active %d)", addr, ovl[i].name,
                     xsg_active_overlay);
            return f;
        }
    }
    xsg_logf("[ovl] no host function for PS2 address 0x%08x (active overlay %d)\n", addr,
             xsg_active_overlay);
    return (void *)unresolved;
}

uint32_t xsg_ps2_addr_of(void *fn)
{
    for (int i = 0; i < NOV; i++)
        for (const struct ovtab_ent *e = ovl[i].tab; e && e->addr; e++)
            if (e->fn == fn)
                return e->addr;
    return 0;
}

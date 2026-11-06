/* Host runtime shared declarations (port/runtime). */
#ifndef XSG_RT_H
#define XSG_RT_H

#include <stdint.h>
#include <stdio.h>

/* ---- logging -------------------------------------------------------- */
extern int xsg_log_level;      /* XSG_LOG=0 quiet, 1 first calls (default), 2 all */
extern FILE *xsg_log_fp;
void xsg_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Log a stub call: every call at level 2, else the first XSG_LOG_FIRST calls
 * per call site and then every 1000th. */
#define XSG_LOG_FIRST 3
#define STUB_LOG(fmt, ...)                                                    \
    do {                                                                      \
        static unsigned xsg_n_;                                               \
        xsg_n_++;                                                             \
        if (xsg_log_level >= 2 ||                                             \
            (xsg_log_level >= 1 &&                                            \
             (xsg_n_ <= XSG_LOG_FIRST || xsg_n_ % 1000 == 0)))                \
            xsg_logf("[%s #%u] " fmt "\n", __func__, xsg_n_, ##__VA_ARGS__);  \
    } while (0)

/* ---- PS2 address space ---------------------------------------------- */
#define PS2_RAM_BASE   0x00000000u
#define PS2_RAM_SIZE   0x02000000u
#define PS2_SPR_BASE   0x70000000u
#define PS2_SPR_SIZE   0x00004000u
#define PS2_EEHW_BASE  0x10000000u
#define PS2_EEHW_SIZE  0x00010000u
#define PS2_VU_BASE    0x11000000u /* VU0 micro 0x11000000, VU0 data 0x11004000, VU1 0x11008000/0x1100C000 */
#define PS2_VU_SIZE    0x00010000u
#define PS2_GSPRIV_BASE 0x12000000u
#define PS2_GSPRIV_SIZE 0x00002000u
extern uint32_t xsg_ram_low;   /* lowest PS2 RAM address actually mapped */
void xsg_map_ps2_memory(void);
void xsg_load_main_image(void);

/* ---- kernel / scheduler --------------------------------------------- */
void xsg_kernel_init(void);
void xsg_vblank(void);          /* advance one frame: callbacks, waiters */
extern volatile unsigned xsg_frame;

/* ---- overlays ------------------------------------------------------- */
void xsg_overlay_init(void);
int xsg_overlay_activate(int id);
extern int xsg_active_overlay;
void *xsg_ps2_func(uint32_t ps2_addr);
uint32_t xsg_ps2_addr_of(void *host_fn);

/* ---- disc ----------------------------------------------------------- */
void xsg_disc_init(void);

#endif

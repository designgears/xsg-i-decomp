/*
 * TU-local declarations of ov12/tu087 (src/ov12/xrg_sysinit.c).
 */

#ifndef SRC_OV12_XRG_SYSINIT_H
#define SRC_OV12_XRG_SYSINIT_H

#include "shared.h"

void XrgSystemInit(void);

/* static const char xrg_system_init_log_1[] (defined in xrg_sysinit.c) */

extern const char xrg_system_init_source_file[];

/* static const char xrg_system_init_log_2[] (defined in xrg_sysinit.c) */

/* static const char xrg_system_init_log_3[] (defined in xrg_sysinit.c) */

/* static const char xrg_system_init_log_4[] (defined in xrg_sysinit.c) */

/* static const char xrg_system_init_log_5[] (defined in xrg_sysinit.c) */

/* static const char xrg_system_init_log_6[] (defined in xrg_sysinit.c) */

/* static const char xrg_system_init_log_7[] (defined in xrg_sysinit.c) */

/* static const char xrg_system_init_log_8[] (defined in xrg_sysinit.c) */

extern void XrgLogSys(const char *, const char *, int, ...);

extern RgHeap *InstanceOfRgHeap(void);

extern RgHeap *InstanceOfRgHeapData(void);

extern void InitRgHeap(RgHeap *, void *, u32);

extern void RgSingletonIDClear(void);

extern void ClearRgHeap(RgHeap *);

static void _SetDefaultLight(void);

extern void xglRenderClearFrame(void);

extern void xglRenderClearColor(u32);

void XrgSleep(void);

#endif /* SRC_OV12_XRG_SYSINIT_H */

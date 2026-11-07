/*
 * Host entry: map the PS2 address space, set up the runtime, then run the
 * game's own main() (src/main/control_entry.c, renamed xsg_game_main at link
 * time) as EE thread 1, the way crt0 would.
 */
#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "xsg_rt.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

int xsg_log_level = 1;
FILE *xsg_log_fp;
uint32_t xsg_ram_low;

void xsg_logf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(xsg_log_fp, fmt, ap);
    va_end(ap);
    fflush(xsg_log_fp);
}

static void *map_fixed(uint32_t addr, uint32_t size, const char *what)
{
    void *p = mmap((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED || p != (void *)(uintptr_t)addr) {
        fprintf(stderr, "xsg: cannot map %s at 0x%08x (+0x%x): %s\n", what, addr,
                size, p == MAP_FAILED ? strerror(errno) : "kernel picked another address");
        exit(1);
    }
    return p;
}

void xsg_map_ps2_memory(void)
{
    /* Page 0 is normally refused (vm.mmap_min_addr, 64 KiB on most distros):
     * map RAM from the lowest address the kernel allows. */
    unsigned long min_addr = 0x10000;
    FILE *f = fopen("/proc/sys/vm/mmap_min_addr", "r");
    if (f) {
        if (fscanf(f, "%lu", &min_addr) != 1)
            min_addr = 0x10000;
        fclose(f);
    }
    min_addr = (min_addr + 0xfff) & ~0xfffUL;
    xsg_ram_low = (uint32_t)min_addr;
    /* Main RAM is a memfd so that the uncached (0x20000000) and uncached-
     * accelerated (0x30000000) views can alias it, as game data holds such
     * pointers (e.g. sef's atanTbl_0 = 0x2040FAC0). */
    int ram_fd = memfd_create("xsg-ee-ram", 0);
    if (ram_fd < 0 || ftruncate(ram_fd, PS2_RAM_SIZE) != 0) {
        fprintf(stderr, "xsg: memfd for EE RAM: %s\n", strerror(errno));
        exit(1);
    }
    for (int view = 0; view < 3; view++) {
        static const uint32_t bases[3] = {0, 0x20000000u, 0x30000000u};
        void *p = mmap((void *)(uintptr_t)(bases[view] + xsg_ram_low), PS2_RAM_SIZE - xsg_ram_low,
                       PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED_NOREPLACE, ram_fd, xsg_ram_low);
        if (p != (void *)(uintptr_t)(bases[view] + xsg_ram_low)) {
            if (view == 0) {
                fprintf(stderr, "xsg: cannot map EE main RAM at 0x%08x\n", xsg_ram_low);
                exit(1);
            }
            xsg_logf("[mem] RAM view at 0x%08x not mapped\n", bases[view]);
        }
    }
    map_fixed(PS2_EEHW_BASE, PS2_EEHW_SIZE, "EE hardware registers");
    map_fixed(PS2_VU_BASE, PS2_VU_SIZE, "VU0/VU1 micro and data memory");
    map_fixed(PS2_GSPRIV_BASE, PS2_GSPRIV_SIZE, "GS privileged registers");
    map_fixed(PS2_SPR_BASE, PS2_SPR_SIZE, "scratchpad");
    xsg_logf("[mem] RAM 0x%08x-0x%08x, EE HW 0x%08x, VU mem 0x%08x, GS priv 0x%08x, SPR 0x%08x\n",
             xsg_ram_low, PS2_RAM_SIZE, PS2_EEHW_BASE, PS2_VU_BASE, PS2_GSPRIV_BASE, PS2_SPR_BASE);
}

/* Copy the original SLUS ELF's loadable segments to their PS2 addresses, so
 * that game code which reads main-image data through hard-coded PS2
 * addresses (e.g. ov01 data_unit_org_get reads 0x426F48) sees the original
 * bytes.  The host's own copy of that data is what C symbols refer to. */
void xsg_load_main_image(void)
{
    const char *path = getenv("XSG_ELF");
    char buf[1024];
    if (!path) {
        const char *root = getenv("XSG_GAME_DIR");
        snprintf(buf, sizeof buf, "%s/extracted-iso/SLUS_204.69", root ? root : XSG_DEFAULT_GAME_DIR);
        path = buf;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        xsg_logf("[mem] %s not found: PS2 RAM image left zeroed\n", path);
        return;
    }
    Elf32_Ehdr eh;
    if (fread(&eh, sizeof eh, 1, f) != 1 || memcmp(eh.e_ident, ELFMAG, SELFMAG)) {
        fclose(f);
        return;
    }
    for (int i = 0; i < eh.e_phnum; i++) {
        Elf32_Phdr ph;
        fseek(f, eh.e_phoff + i * eh.e_phentsize, SEEK_SET);
        if (fread(&ph, sizeof ph, 1, f) != 1 || ph.p_type != PT_LOAD)
            continue;
        if (ph.p_vaddr < xsg_ram_low || ph.p_vaddr + ph.p_memsz > PS2_RAM_SIZE)
            continue;
        fseek(f, ph.p_offset, SEEK_SET);
        if (fread((void *)(uintptr_t)ph.p_vaddr, 1, ph.p_filesz, f) != ph.p_filesz)
            xsg_logf("[mem] short read of segment %d\n", i);
        xsg_logf("[mem] %s segment %d -> 0x%08x (+0x%x)\n", path, i, ph.p_vaddr, ph.p_memsz);
    }
    fclose(f);
}

/* Stand-in for the DMA controller: the EE register range is plain memory, so
 * a channel's CHCR.STR (bit 8) would stay set forever after the game starts a
 * transfer and polls for completion.  Every DMA "completes" immediately: a
 * host thread keeps clearing STR.  VIF1 (ch1) and GIF (ch2)
 * transfers are run first, into the software GS (gs_bridge.cpp). */
void xsg_dma_run(int ch, uint32_t base);

static void *dmac_thread(void *arg)
{
    static const uint32_t chcr[] = {0x10008000, 0x10009000, 0x1000A000, 0x1000B000, 0x1000B400,
                                    0x1000C000, 0x1000C400, 0x1000C800, 0x1000D000, 0x1000D400};
    struct timespec ts = {0, 50000};
    const char *e = getenv("XSG_DMA_THREAD");
    const int async_gfx = e && atoi(e);
    (void)arg;
    for (;;) {
        for (unsigned i = 0; i < sizeof chcr / sizeof chcr[0]; i++) {
            volatile uint32_t *r = (volatile uint32_t *)(uintptr_t)chcr[i];
            /* VIF1 and GIF run synchronously on the game's thread (xsg_dma_kick
               from xglDmaDirect*, gs_bridge.cpp); XSG_DMA_THREAD=1 brings back
               Stage 5's asynchronous run here. */
            if (i == 1 || i == 2) {
                if (async_gfx && (*r & 0x100))
                    xsg_dma_run(i, chcr[i]);
                continue;
            }
            if (*r & 0x100)
                __atomic_fetch_and(r, ~0x100u, __ATOMIC_SEQ_CST);
        }
        nanosleep(&ts, NULL);
    }
    return NULL;
}

extern int xsg_game_main(int argc, int argv);
extern void xsg_kernel_dump(void);

/* XSG_TIMEOUT=<s>: after that many seconds, log where the game is and exit
 * (headless runs have no other way to tell a busy-wait from progress). */
static void on_alarm(int sig)
{
    void *bt[32];
    int n = backtrace(bt, 32);
    xsg_logf("[timeout] frame %u, overlay %d; backtrace of the running EE thread:\n", xsg_frame,
             xsg_active_overlay);
    backtrace_symbols_fd(bt, n, fileno(xsg_log_fp));
    xsg_kernel_dump();
    _exit(124);
    (void)sig;
}

static void on_fault(int sig, siginfo_t *si, void *uc)
{
    (void)uc;
    xsg_logf("[fatal] signal %d at address %p (frame %u, overlay %d)\n", sig, si->si_addr,
             xsg_frame, xsg_active_overlay);
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const char *lv = getenv("XSG_LOG");
    xsg_log_fp = stderr;
    if (lv)
        xsg_log_level = atoi(lv);
    if (getenv("XSG_LOG_FILE")) {
        xsg_log_fp = fopen(getenv("XSG_LOG_FILE"), "w");
        if (!xsg_log_fp)
            xsg_log_fp = stderr;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    static char altstack[65536];
    stack_t ss = {.ss_sp = altstack, .ss_size = sizeof altstack};
    sigaltstack(&ss, NULL);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);

    if (getenv("XSG_TIMEOUT")) {
        signal(SIGALRM, on_alarm);
        alarm(atoi(getenv("XSG_TIMEOUT")));
    }
    xsg_map_ps2_memory();
    xsg_load_main_image();
    pthread_t dmac;
    pthread_create(&dmac, NULL, dmac_thread, NULL);
    xsg_overlay_init();
    xsg_disc_init();
    xsg_kernel_init();
    xsg_logf("[boot] entering game main()\n");
    int r = xsg_game_main(0, 0);
    xsg_logf("[boot] game main() returned %d\n", r);
    return r;
}

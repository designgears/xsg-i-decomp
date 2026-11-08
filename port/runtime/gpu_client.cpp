/*
 * Stage 9: game side of the shared-memory ring to xsg-gpu (protocol in
 * gpu_ipc.h, design in port/docs/STAGE9_GPU.md).
 *
 * GIF data is appended in place into an open GPU_MSG_GIF message (up to
 * kChunk bytes reserved contiguously in the ring); the message is closed and
 * published when the path changes, when it is full, or before any other
 * message.  So the consumer sees exactly the order of the producer's calls,
 * and the data is copied out of game memory at the time of the DMA kick.
 */
#include "gpu_client.h"
#include "xsg_settings.h"
#include "gpu_ipc.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <strings.h>
#include <ctime>
#include <fcntl.h>
#include <linux/futex.h>
#include <spawn.h>
#include <string>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern "C" {
#include "xsg_rt.h"
}

extern char** environ;

namespace xsggpu {
namespace {

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kMask = XSG_GPU_RING - 1;
constexpr u32 kChunk = 64u << 10;
constexpr u32 kSlack = 1024u << 10;  // a PACKED packet may run past kChunk by this much

GpuShm* shm;
u8* ring;
u8* reply;
pid_t child = -1;
u32 wpos;
bool open_msg;
u32 open_at, open_len;
int open_path;
u32 seq;
u64 priv_sent[0x20];
bool priv_valid[0x20];
u64 stalls;

long Futex(volatile u32* a, int op, u32 v, const timespec* ts) {
    return syscall(SYS_futex, (u32*)a, op, v, ts, nullptr, 0);
}

void CheckChild() {
    int status = 0;
    if (child > 0 && waitpid(child, &status, WNOHANG) == child) {
        const int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        xsg_logf("[gpu] xsg-gpu exited (status %d); the game stops too\n", code);
        std::_Exit(code);
    }
}

void Publish() {
    __atomic_store_n(&shm->wpos, wpos, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&shm->wpos_waiting, __ATOMIC_SEQ_CST)) Futex(&shm->wpos, FUTEX_WAKE, 1, nullptr);
}

/* n contiguous free bytes at wpos (padding the ring's tail if needed).
   Blocks while the ring is full: back-pressure, never a drop. */
void Reserve(u32 n) {
    const u32 tail = XSG_GPU_RING - (wpos & kMask);
    const u32 need = n > tail ? n + tail : n;
    for (;;) {
        u32 rpos = __atomic_load_n(&shm->rpos, __ATOMIC_ACQUIRE);
        if (XSG_GPU_RING - (wpos - rpos) >= need) break;
        stalls++;
        __atomic_store_n(&shm->rpos_waiting, 1u, __ATOMIC_SEQ_CST);
        rpos = __atomic_load_n(&shm->rpos, __ATOMIC_SEQ_CST);
        if (XSG_GPU_RING - (wpos - rpos) < need) {
            timespec ts = {0, 50 * 1000 * 1000};
            Futex(&shm->rpos, FUTEX_WAIT, rpos, &ts);
            CheckChild();
        }
        __atomic_store_n(&shm->rpos_waiting, 0u, __ATOMIC_SEQ_CST);
    }
    if (n > tail) {
        GpuMsg pad = {GPU_MSG_PAD, 0, 0, 0};
        std::memcpy(ring + (wpos & kMask), &pad, sizeof pad);
        wpos += tail;
        Publish();
    }
}

void Close() {
    if (!open_msg) return;
    open_msg = false;
    GpuMsg h = {GPU_MSG_GIF, open_len, u32(open_path), 0};
    std::memcpy(ring + (open_at & kMask), &h, sizeof h);
    wpos = open_at + 16 + open_len;
    Publish();
}

void Send(u32 type, u32 a, u32 b, const void* payload, u32 len) {
    Close();
    const u32 padded = (len + 15) & ~15u;
    Reserve(16 + padded);
    GpuMsg h = {type, padded, a, b};
    std::memcpy(ring + (wpos & kMask), &h, sizeof h);
    if (len) std::memcpy(ring + ((wpos + 16) & kMask), payload, len);
    wpos += 16 + padded;
    Publish();
}

void WaitReply(u32 s) {
    for (;;) {
        u32 r = __atomic_load_n(&shm->reply_seq, __ATOMIC_ACQUIRE);
        if (r == s) return;
        timespec ts = {0, 50 * 1000 * 1000};
        Futex(&shm->reply_seq, FUTEX_WAIT, r, &ts);
        CheckChild();
    }
}

std::string DefaultBin() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return "xsg-gpu";
    buf[n] = 0;
    std::string s(buf);
    return s.substr(0, s.rfind('/')) + "/gpu/xsg-gpu";
}

}  // namespace

bool Active() { return shm != nullptr; }

bool Start(const char* stream) {
    int fd = memfd_create("xsg-gpu-ring", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, XSG_GPU_SHM_SIZE) != 0) {
        xsg_logf("[gpu] memfd: %s\n", std::strerror(errno));
        return false;
    }
    void* m = mmap(nullptr, XSG_GPU_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m == MAP_FAILED) {
        xsg_logf("[gpu] mmap of %u bytes: %s\n", XSG_GPU_SHM_SIZE, std::strerror(errno));
        return false;
    }
    GpuShm* s = static_cast<GpuShm*>(m);
    std::memset(s, 0, sizeof *s);
    s->magic = XSG_GPU_MAGIC;
    s->version = XSG_GPU_VERSION;
    s->ring_size = XSG_GPU_RING;
    s->reply_size = XSG_GPU_REPLY;
    s->reply_seq = ~0u;

    // xsg-gpu reads the same settings.ini (XSG_CONFIG is inherited); the
    // live-stream FIFO, which only one process may feed, goes on its command line.
    const char* bin_cfg = xsg_cfg("debug.gpu_bin");
    std::string bin = bin_cfg ? bin_cfg : DefaultBin();
    std::string fd_arg = std::to_string(XSG_GPU_FD), stream_arg = std::string("--stream=") + (stream ? stream : "");
    char* argv[] = {&bin[0], &fd_arg[0], stream ? &stream_arg[0] : nullptr, nullptr};
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fd, XSG_GPU_FD);
    pid_t pid;
    // DLSS 5 Neural Rendering ([renderer] neural, off by default): the DLSS5VKLayer
    // implicit layer activates only with VKLayer_DLSS5=1, given when the setting is on
    // at start and the layer is installed (turning it on later takes a restart; off
    // again is live, through the helper's shared settings).
    std::vector<char*> env;
    for (char** e = environ; *e; ++e)
        if (std::strncmp(*e, "VKLayer_DLSS5=", 14)) env.push_back(*e);
    static char neural_on[] = "VKLayer_DLSS5=1";
    bool layer = false;
    {
        const char* home = getenv("HOME");
        const char* xdg = getenv("XDG_DATA_HOME");
        const std::string m = "/vulkan/implicit_layer.d/VK_LAYER_NV_dlssnr.x86_64.json";
        for (const std::string& d : {std::string(xdg && *xdg ? xdg : std::string(home ? home : "") + "/.local/share"),
                                     std::string("/usr/share"), std::string("/usr/local/share"), std::string("/etc")})
            layer |= access((d + m).c_str(), R_OK) == 0;
    }
    // VKLayer_DLSS5=0 already in the environment keeps the layer out (a second instance,
    // e.g. a test run, would otherwise attach to the one helper the main game uses).
    const char* forced = getenv("VKLayer_DLSS5");
    if (forced && std::strcmp(forced, "0") == 0) layer = false;
    {
        const char* nr = xsg_cfg("renderer.neural");
        if (!(nr && std::atoi(nr))) layer = false;   // off: no layer in the present path at all
    }
    if (layer) {
        env.push_back(neural_on);
        xsg_logf("[gpu] DLSS 5 Neural Rendering on: DLSS5VKLayer loaded\n");
    }
    env.push_back(nullptr);
    int rc = posix_spawn(&pid, bin.c_str(), &fa, nullptr, argv, env.data());
    posix_spawn_file_actions_destroy(&fa);
    close(fd);
    if (rc != 0) {
        xsg_logf("[gpu] cannot start %s: %s ([debug] gpu_bin in settings.ini overrides the path)\n", bin.c_str(), std::strerror(rc));
        return false;
    }
    child = pid;
    shm = s;
    ring = static_cast<u8*>(m) + XSG_GPU_HDR;
    reply = ring + XSG_GPU_RING;
    for (int i = 0; i < 400; ++i) {    // up to 20 s for the Vulkan device
        u32 r = __atomic_load_n(&s->ready, __ATOMIC_ACQUIRE);
        if (r == 1) break;
        if (r == 2) { xsg_logf("[gpu] xsg-gpu failed to start its renderer\n"); std::_Exit(1); }
        timespec ts = {0, 50 * 1000 * 1000};
        Futex(&s->ready, FUTEX_WAIT, r, &ts);
        CheckChild();
    }
    if (s->ready != 1) { xsg_logf("[gpu] xsg-gpu did not come up\n"); std::_Exit(1); }
    xsg_logf("[gpu] xsg-gpu pid %d up (%s), ring %u MB\n", int(pid), bin.c_str(), XSG_GPU_RING >> 20);
    return true;
}

/* GIF packet position per path, so a message is only cut where paraLLEl-GS
   copes with it: on a tag boundary or inside IMAGE data.  (Its PACKED fast
   path sizes a batch from the whole call, `size / nreg`, not what is left of
   it, so a call that ends inside a PACKED packet overruns.) */
struct PathState { u32 left = 0; bool image = false; };
PathState pstate[4];

void Track(PathState& ps, u64 lo) {
    // lo is a GIF tag: the body length that follows it.
    const u32 nloop = u32(lo & 0x7fff), flg = u32(lo >> 58) & 3;
    u32 nreg = u32(lo >> 60) & 15;
    if (!nreg) nreg = 16;
    ps.image = flg >= 2;
    ps.left = flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
}

/* Room for one more qword on `path` in the open message, opening a new one
   if needed.  Returns how many qwords may be appended in one run. */
u32 Room(int path) {
    const PathState& ps = pstate[path & 3];
    const bool can_cut = ps.left == 0 || ps.image;
    const u32 limit = can_cut ? kChunk : kChunk + kSlack;
    if (!open_msg || open_path != path || open_len + 16 > limit) {
        if (open_msg && open_path == path && !can_cut) {
            static bool told;
            if (!told) { told = true; xsg_logf("[gpu] a PACKED GIF packet over %u KB was cut\n", kSlack >> 10); }
        }
        Close();
        Reserve(16 + kChunk + kSlack);
        open_msg = true;
        open_at = wpos;
        open_len = 0;
        open_path = path;
        return kChunk / 16;
    }
    return (limit - open_len) / 16;
}

void Append(const u8* b, u32 k) {
    const u32 at = (open_at + 16 + open_len) & kMask;
    std::memcpy(ring + at, b, size_t(k) * 16);   // Reserve made it contiguous
    open_len += k * 16;
}

void GifBlock(int path, const void* p, u32 n) {
    const u8* b = static_cast<const u8*>(p);
    PathState& ps = pstate[path & 3];
    while (n) {
        u32 room = Room(path);
        if (ps.left == 0) {                 // a tag
            u64 lo;
            std::memcpy(&lo, b, 8);
            Append(b, 1);
            Track(ps, lo);
            b += 16; n--;
            continue;
        }
        u32 k = std::min(std::min(room, n), ps.left);
        Append(b, k);
        ps.left -= k;
        b += size_t(k) * 16;
        n -= k;
    }
}

void Gif(int path, u64 lo, u64 hi) {
    u8 q[16];
    std::memcpy(q, &lo, 8);
    std::memcpy(q + 8, &hi, 8);
    GifBlock(path, q, 1);
}

void SyncPriv() {
    // PMODE..BGCOLOR (0x0000-0x00E0) and CSR/IMR/BUSDIR/SIGLBLID (0x1000-0x1080).
    for (u32 i = 0; i < 0x20; ++i) {
        const u32 off = i < 0x10 ? i * 0x10 : 0x1000 + (i - 0x10) * 0x10;
        if (i >= 0x19) break;
        const u64 v = *reinterpret_cast<volatile u64*>(uintptr_t(PS2_GSPRIV_BASE + off));
        if (priv_valid[i] && priv_sent[i] == v) continue;
        priv_valid[i] = true;
        priv_sent[i] = v;
        Send(GPU_MSG_PRIV, off, 0, &v, 8);
    }
}

void Vsync(u32 frame, u32 field) {
    SyncPriv();
    Send(GPU_MSG_VSYNC, frame, field, nullptr, 0);
    static u32 n;
    if (++n % 600 == 0)
        xsg_logf("[gpu] vblank %u: renderer at vsync %u, ring %u KB queued, %llu full-ring stalls\n", frame,
                 __atomic_load_n(&shm->vsyncs_done, __ATOMIC_ACQUIRE),
                 (wpos - __atomic_load_n(&shm->rpos, __ATOMIC_ACQUIRE)) >> 10, (unsigned long long)stalls);
}

const u8* ReadVram() {
    SyncPriv();
    const u32 s = ++seq;
    Send(GPU_MSG_READ_VRAM, s, 0, nullptr, 0);
    WaitReply(s);
    return reply;
}

void ReadFifo(void* dst, u32 qwords) {
    u8* d = static_cast<u8*>(dst);
    while (qwords) {
        const u32 n = std::min(qwords, XSG_GPU_REPLY / 16);
        const u32 s = ++seq;
        Send(GPU_MSG_READ_FIFO, s, n, nullptr, 0);
        WaitReply(s);
        std::memcpy(d, reply, size_t(n) * 16);
        d += size_t(n) * 16;
        qwords -= n;
    }
}

void Flush() { Close(); }

void Native(u32 kind, u32 nvert, const void* payload, u32 len) { Send(GPU_MSG_NATIVE, kind, nvert, payload, len); }

/* Stage 10: a consistent copy of the live pad block (seqlock read). */
int PadRead(unsigned* buttons, unsigned char stick[4], unsigned char press[12]) {
    if (!shm) return 0;
    const GpuPad* p = reinterpret_cast<const GpuPad*>(reinterpret_cast<const u8*>(shm) + XSG_GPU_PAD_OFF);
    for (int tries = 0; tries < 1000; ++tries) {
        const u32 s0 = __atomic_load_n(&p->seq, __ATOMIC_ACQUIRE);
        if (s0 & 1) continue;
        if (!p->active) return 0;
        const u32 b = p->buttons;
        u8 st[4], pr[12];
        for (int i = 0; i < 4; ++i) st[i] = u8(p->stick[i]);
        for (int i = 0; i < 12; ++i) pr[i] = u8(p->press[i]);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&p->seq, __ATOMIC_RELAXED) != s0) continue;
        *buttons = b;
        std::memcpy(stick, st, 4);
        std::memcpy(press, pr, 12);
        return 1;
    }
    return 0;
}

}  // namespace xsggpu

namespace xsggpu {
/* In-game menu: a consistent copy of the settings block; 1 when valid. */
int SettingsRead(GpuSettings* out) {
    if (!shm) return 0;
    const GpuSettings* p = reinterpret_cast<const GpuSettings*>(reinterpret_cast<const u8*>(shm) + XSG_GPU_SETTINGS_OFF);
    for (int tries = 0; tries < 1000; ++tries) {
        const u32 s0 = __atomic_load_n(&p->seq, __ATOMIC_ACQUIRE);
        if (s0 & 1) continue;
        if (!p->valid) return 0;
        GpuSettings c;
        std::memcpy(&c, (const void*)p, sizeof c);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&p->seq, __ATOMIC_RELAXED) != s0) continue;
        *out = c;
        return 1;
    }
    return 0;
}

/* "Restart now": end xsg-gpu, then exec this program again with the same
   command line; the new run reads settings.ini afresh. */
[[noreturn]] void Restart() {
    xsg_logf("[settings] restart requested by the menu\n");
    if (child > 0) {
        kill(child, SIGTERM);
        waitpid(child, nullptr, 0);
    }
    std::vector<std::string> args;
    if (FILE* f = std::fopen("/proc/self/cmdline", "rb")) {
        std::string cur;
        int c;
        while ((c = std::fgetc(f)) != EOF) {
            if (c == 0) { args.push_back(cur); cur.clear(); }
            else cur += char(c);
        }
        std::fclose(f);
    }
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(&a[0]);
    argv.push_back(nullptr);
    std::fflush(nullptr);
    execv("/proc/self/exe", argv.data());
    xsg_logf("[settings] restart failed: %s\n", std::strerror(errno));
    std::_Exit(1);
}

}  // namespace xsggpu

/* Returns 0 when xsg-gpu is not running or publishes no input (headless). */
extern "C" int xsg_gpu_pad_read(unsigned* buttons, unsigned char stick[4], unsigned char press[12]) {
    return xsggpu::PadRead(buttons, stick, press);
}

/* Once per vblank (kernel.c): the menu's live settings.  Returns 1 and fills
   the out values when they changed since the last call (frame_us may be
   XSG_GPU_FRAME_US_KEEP); a restart request does not return. */
extern "C" int xsg_gpu_settings_poll(unsigned* frame_us, int* audio_on, int vol[4]) {
    static GpuSettings last;
    static bool have;
    GpuSettings s;
    if (!xsggpu::SettingsRead(&s)) return 0;
    if (s.restart) xsggpu::Restart();
    if (have && s.frame_us == last.frame_us && s.audio_on == last.audio_on &&
        std::memcmp((const void*)s.vol, (const void*)last.vol, sizeof s.vol) == 0)
        return 0;
    last = s;
    have = true;
    *frame_us = s.frame_us;
    *audio_on = int(s.audio_on);
    for (int i = 0; i < 4; ++i) vol[i] = int(s.vol[i]);
    return 1;
}

/* Rebuilt UI (gpu_ipc.h GpuUiDialog): the dialog window, from twsys_init.c /
   ew_sprt_set_cursor_uv.c under PORT_HOST.  ui_on: [display] ui = rebuilt. */
extern "C" { void* xsg_ui_dialog_widget; }
extern "C" int xsg_ui_rebuilt(void) {
    static unsigned gen = ~0u;
    static int on;
    const unsigned g = __atomic_load_n(&xsg_cfg_gen, __ATOMIC_ACQUIRE);
    if (g != gen) {
        gen = g;
        const char* v = xsg_cfg("display.ui");
        on = v && !std::strcmp(v, "rebuilt");
    }
    return on && xsggpu::shm;
}
static GpuUiDialog* UiDialog() {
    return reinterpret_cast<GpuUiDialog*>(reinterpret_cast<uint8_t*>(xsggpu::shm) + XSG_GPU_UI_OFF);
}
extern "C" void xsg_ui_dialog_window(int x, int y, int w, int h, unsigned state, const char* name) {
    if (!xsggpu::shm) return;
    GpuUiDialog* d = UiDialog();
    const uint32_t s = d->seq;
    __atomic_store_n(&d->seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    d->x = x; d->y = y; d->w = w; d->h = h;
    d->state = state;
    std::strncpy(const_cast<char*>(d->name), name ? name : "", sizeof d->name - 1);
    d->frame = xsg_frame;
    __atomic_store_n(&d->seq, s + 2, __ATOMIC_RELEASE);
}
extern "C" void xsg_ui_dialog_lines(const char* const* lines, int first, int end) {
    if (!xsggpu::shm) return;
    GpuUiDialog* d = UiDialog();
    const uint32_t s = d->seq;
    __atomic_store_n(&d->seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    int n = 0;
    for (int i = first; i < end && n < XSG_GPU_UI_LINES; ++i, ++n)
        std::strncpy(d->line[n], lines[i] ? lines[i] : "", XSG_GPU_UI_LINE - 1);
    for (int k = n; k < XSG_GPU_UI_LINES; ++k) d->line[k][0] = 0;
    d->nlines = uint32_t(n);
    __atomic_store_n(&d->seq, s + 2, __ATOMIC_RELEASE);
}

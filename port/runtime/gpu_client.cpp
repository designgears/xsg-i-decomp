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

/* Rebuilt UI: one battle party panel (ov01 menu.c, PORT_HOST). */
/* Capture mode (menu.c menuKeyGuideDisp, battle_init.c monsNameDisp): ops collect here
   and go to the channel's shared-memory block in one seqlocked copy. */
extern "C" { int xsg_ui_kg_capture; }
static GpuUiKgOp g_kgbuf[XSG_GPU_UI_KG_CHANNELS][XSG_GPU_UI_KG_OPS];
static uint32_t g_kgcount[XSG_GPU_UI_KG_CHANNELS], g_kgframe[XSG_GPU_UI_KG_CHANNELS];
static int g_kg_ch;
#define g_kg g_kgbuf[g_kg_ch]
#define g_kg_n g_kgcount[g_kg_ch]
static uint64_t g_cap_tex0, g_cap_alpha = 0x44;
extern "C" void xsg_ui_cap_begin(int channel) {
    g_kg_ch = channel & (XSG_GPU_UI_KG_CHANNELS - 1);
    g_kg_n = 0;
    g_kgframe[g_kg_ch] = xsg_frame;
    xsg_ui_kg_capture = 1;
    g_cap_alpha = 0x44;
}
/* The same, but several calls in one frame add up (one per party member, ...). */
extern "C" void xsg_ui_cap_append(int channel) {
    g_kg_ch = channel & (XSG_GPU_UI_KG_CHANNELS - 1);
    if (g_kgframe[g_kg_ch] != xsg_frame) g_kg_n = 0;
    g_kgframe[g_kg_ch] = xsg_frame;
    xsg_ui_kg_capture = 1;
    g_cap_alpha = 0x44;
}
extern "C" void xsg_ui_kg_begin(void) { xsg_ui_cap_begin(0); }
extern "C" void xsg_ui_kg_op(int type, int x, int y, int a, int b, const char* text) {
    if (g_kg_n >= XSG_GPU_UI_KG_OPS) return;
    GpuUiKgOp& o = g_kg[g_kg_n++];
    o.type = type; o.x = x; o.y = y; o.a = a; o.b = b;
    std::memset(o.text, 0, sizeof o.text);
    if (text) std::strncpy(o.text, text, sizeof o.text - 1);
}
/* A captured texture (TEX0) gets a snapshot slot: xsg-gpu copies its pages and CLUT from the
   GS-memory mirror when the stream reaches the marker, into texture set 0x5B000000 + slot (one
   slot per texture, 8 in all), so art whose VRAM is reused later in the frame stays right. */
extern "C" int xsg_ui_mirror_on(void);
static uint32_t g_cap_set;
extern "C" unsigned xsg_ui_cap_texmark(unsigned long long tex0) {
    const uint64_t key = (tex0 & 0x3fffull) ^ ((tex0 >> 37) & 0x3fffull) * 0x9e37ull;
    const unsigned slot = unsigned(key % 8);
    g_cap_set = 0x5B000000u + slot;
    return 0x200u | slot;
}
extern "C" void xsg_ui_cap_reg(int reg, unsigned long long value) {
    if (reg == 6) g_cap_tex0 = value;
    else if (reg == 0x42) g_cap_alpha = value;
}
extern "C" void xsg_ui_cap_spr(int x0, int y0, int x1, int y1, int u0, int v0, int u1, int v1, const unsigned char* col) {
    if (g_kg_n >= XSG_GPU_UI_KG_OPS) return;
    GpuUiKgOp& o = g_kg[g_kg_n++];
    std::memset(&o, 0, sizeof o);
    o.type = GPU_KG_ART;
    o.x = x0; o.y = y0; o.dw = int16_t(x1 - x0); o.dh = int16_t(y1 - y0);
    o.u = int16_t(u0); o.v = int16_t(v0); o.w = int16_t(u1 - u0); o.h = int16_t(v1 - v0);
    o.tex0_lo = uint32_t(g_cap_tex0); o.tex0_hi = uint32_t(g_cap_tex0 >> 32);
    if (xsg_ui_mirror_on()) std::memcpy(o.text + 4, &g_cap_set, 4);   // its texture set (0: GS memory)
    o.a = int32_t(g_cap_alpha & 0xff);
    o.b = int32_t((g_cap_alpha >> 32) & 0xff);   // FIX
    o.rgba = uint32_t(col[0]) | uint32_t(col[1]) << 8 | uint32_t(col[2]) << 16 | uint32_t(col[3]) << 24;
}
extern "C" void xsg_ui_cap_quad(const int* xs, const int* ys, const float* us, const float* vs, const unsigned char* col) {
    if (g_kg_n >= XSG_GPU_UI_KG_OPS) return;
    GpuUiKgOp& o = g_kg[g_kg_n++];
    std::memset(&o, 0, sizeof o);
    o.type = GPU_KG_QUAD;
    o.x = xs[0]; o.y = ys[0];
    int16_t c[6] = {int16_t(xs[1]), int16_t(ys[1]), int16_t(xs[2]), int16_t(ys[2]), int16_t(xs[3]), int16_t(ys[3])};
    std::memcpy(o.text, c, sizeof c);
    o.text[12] = char(g_cap_alpha & 0xff);
    o.text[13] = char((g_cap_alpha >> 32) & 0xff);
    o.u = int16_t(us[0]); o.v = int16_t(vs[0]); o.w = int16_t(us[1]); o.h = int16_t(vs[1]);
    o.dw = int16_t(us[2]); o.dh = int16_t(vs[2]);
    o.a = int32_t(uint16_t(int16_t(us[3]))) | int32_t(uint32_t(uint16_t(int16_t(vs[3]))) << 16);
    o.b = xsg_ui_mirror_on() ? int32_t(g_cap_set) : 0;
    o.tex0_lo = uint32_t(g_cap_tex0); o.tex0_hi = uint32_t(g_cap_tex0 >> 32);
    o.rgba = uint32_t(col[0]) | uint32_t(col[1]) << 8 | uint32_t(col[2]) << 16 | uint32_t(col[3]) << 24;
}
extern "C" void xsg_ui_cap_line(int x0, int y0, int x1, int y1, const unsigned char* col) {
    if (g_kg_n >= XSG_GPU_UI_KG_OPS) return;
    GpuUiKgOp& o = g_kg[g_kg_n++];
    std::memset(&o, 0, sizeof o);
    o.type = GPU_KG_LINE;
    o.x = x0; o.y = y0; o.dw = int16_t(x1 - x0); o.dh = int16_t(y1 - y0);
    o.rgba = uint32_t(col[0]) | uint32_t(col[1]) << 8 | uint32_t(col[2]) << 16 | uint32_t(col[3]) << 24;
}
extern "C" void xsg_ui_kg_end(void) {
    xsg_ui_kg_capture = 0;
    if (!xsggpu::shm || g_kg_ch < 0 || g_kg_ch >= XSG_GPU_UI_KG_CHANNELS) return;
    GpuUiKeyGuide* k = reinterpret_cast<GpuUiKeyGuide*>(reinterpret_cast<uint8_t*>(xsggpu::shm) + XSG_GPU_UI_KG_OFF) + g_kg_ch;
    const uint32_t s = k->seq;
    __atomic_store_n(&k->seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    std::memcpy(k->op, g_kg, sizeof(GpuUiKgOp) * g_kg_n);
    k->nops = g_kg_n;
    k->frame = xsg_frame;
    __atomic_store_n(&k->seq, s + 2, __ATOMIC_RELEASE);
}
extern "C" void xsg_ui_cap_end(void) { xsg_ui_kg_end(); }
extern "C" void xsg_ui_dmg(int key, int x, int y, int value, int type, int count) {
    if (!xsggpu::shm) return;
    GpuUiDmgList* l = reinterpret_cast<GpuUiDmgList*>(reinterpret_cast<uint8_t*>(xsggpu::shm) + XSG_GPU_UI_DMG_OFF);
    int slot = -1;
    for (int i = 0; i < XSG_GPU_UI_DMGS && slot < 0; ++i)
        if (l->d[i].key == key && l->d[i].frame && xsg_frame - l->d[i].frame < 8) slot = i;
    for (int i = 0; i < XSG_GPU_UI_DMGS && slot < 0; ++i)
        if (!l->d[i].frame || xsg_frame - l->d[i].frame > 8) slot = i;
    if (slot < 0) return;
    const uint32_t s = l->seq;
    __atomic_store_n(&l->seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    GpuUiDmg& d = l->d[slot];
    d.key = key; d.x = x; d.y = y; d.value = value; d.type = type; d.count = count;
    d.frame = xsg_frame;
    __atomic_store_n(&l->seq, s + 2, __ATOMIC_RELEASE);
}
extern "C" void xsg_ui_battle_cmd(int x, int y, int sel, int sel_cnt, int agws, const int* rows) {
    if (!xsggpu::shm) return;
    GpuUiBattleCmd* c =
        reinterpret_cast<GpuUiBattleCmd*>(reinterpret_cast<uint8_t*>(xsggpu::shm) + XSG_GPU_UI_CMD_OFF);
    const uint32_t s = c->seq;
    __atomic_store_n(&c->seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    c->x = x; c->y = y; c->sel = sel; c->sel_cnt = sel_cnt; c->agws = agws;
    for (int i = 0; i < 5; ++i) {
        GpuUiCmdRow& r = c->row[i];
        r.label = rows[i * 5]; r.cmd = rows[i * 5 + 1]; r.enabled = rows[i * 5 + 2];
        r.x = rows[i * 5 + 3]; r.y = rows[i * 5 + 4];
    }
    c->frame = xsg_frame;
    __atomic_store_n(&c->seq, s + 2, __ATOMIC_RELEASE);
}
extern "C" void xsg_ui_battle_unit(int key, int x, int y, int active, int hp, int hp_max, int ep, int ep_max, int ap,
                                   int bg, int bp, int idol) {
    if (!xsggpu::shm) return;
    GpuUiBattle* b = reinterpret_cast<GpuUiBattle*>(reinterpret_cast<uint8_t*>(xsggpu::shm) + XSG_GPU_UI_BATTLE_OFF);
    int slot = -1;
    for (int i = 0; i < XSG_GPU_UI_UNITS && slot < 0; ++i)
        if (b->unit[i].key == key && b->unit[i].frame) slot = i;
    for (int i = 0; i < XSG_GPU_UI_UNITS && slot < 0; ++i)   // a free or stale slot
        if (!b->unit[i].frame || xsg_frame - b->unit[i].frame > 30) slot = i;
    if (slot < 0) return;
    const uint32_t s = b->seq;
    __atomic_store_n(&b->seq, s + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    GpuUiUnit& u = b->unit[slot];
    u.key = key; u.x = x; u.y = y; u.active = active;
    u.hp = hp; u.hp_max = hp_max; u.ep = ep; u.ep_max = ep_max; u.ap = ap; u.bg = bg; u.bp = bp; u.idol = idol;
    u.frame = xsg_frame;
    __atomic_store_n(&b->seq, s + 2, __ATOMIC_RELEASE);
}

/* ---- rebuilt UI: the menus' display list (end_print.c xsg_ui_print_capture) ---------- */
namespace {
GpuUiKgOp g_mops[XSG_GPU_UI_MENU_OPS];
uint32_t g_mop_n;
uint32_t g_mop_start;    // the frame the buffer started filling
uint32_t g_menu_last;    // the frame of the last captured flush
uint32_t g_published_frame;
int g_last_text_op = -1;
uint32_t g_last_text_frame;
int g_last_x, g_last_y, g_last_param;
char g_mpool[XSG_GPU_UI_MENU_POOL];
uint32_t g_mpool_n;
}  // namespace

extern "C" int xsg_ui_mop(int type, int x, int y, int w, int h, int z, unsigned rgba, int b) {
    if (g_mop_n && xsg_frame - g_mop_start > 2) g_mop_n = 0, g_mpool_n = 0;   // a stale partial frame
    if (!g_mop_n) g_mop_start = xsg_frame;
    if (g_mop_n >= XSG_GPU_UI_MENU_OPS) return -1;
    GpuUiKgOp& o = g_mops[g_mop_n];
    std::memset(&o, 0, sizeof o);
    o.type = type; o.x = x; o.y = y; o.dw = int16_t(w); o.dh = int16_t(h);
    o.a = z; o.b = b; o.rgba = rgba;
    return int(g_mop_n++);
}
extern "C" void xsg_ui_mop_art(int op, unsigned tex0_lo, unsigned tex0_hi, int u, int v, int tw, int th, unsigned texset) {
    if (op < 0) return;
    GpuUiKgOp& o = g_mops[op];
    o.tex0_lo = tex0_lo; o.tex0_hi = tex0_hi;
    o.u = int16_t(u); o.v = int16_t(v); o.w = int16_t(tw); o.h = int16_t(th);
    o.b = int32_t(texset);
}
/* The last op is an inline text icon (e_message.c msg_spr[slot]): text[0] = 1, text[1] = slot. */
extern "C" void xsg_ui_mop_icon(int slot) {
    if (!g_mop_n) return;
    GpuUiKgOp& o = g_mops[g_mop_n - 1];
    o.text[0] = 1;
    o.text[1] = char(slot);
}
/* ART: text[2] = 1 + uv table, text[3] = entry (end_print.c uv_clut). */
extern "C" void xsg_ui_mop_uvid(int op, int uvIndex) {
    if (op < 0) return;
    g_mops[op].text[2] = char(1 + ((uvIndex >> 8) & 0x7f));
    g_mops[op].text[3] = char(uvIndex & 0xff);
}
extern "C" void xsg_ui_mop_uv(int op, int u, int v) {
    if (op < 0) return;
    g_mops[op].u = int16_t(u);
    g_mops[op].v = int16_t(v);
}
extern "C" void xsg_ui_mop_text(int op, const char* text, int len) {
    if (op < 0 || len < 0) return;
    if (g_mpool_n + uint32_t(len) + 1 > XSG_GPU_UI_MENU_POOL) len = 0;
    GpuUiKgOp& o = g_mops[op];
    if (o.type != GPU_KG_TEXT) o.w = int16_t(o.b);   // TAG: keep the right-aligned flag
    o.b = int32_t(g_mpool_n);
    o.u = int16_t(len);
    std::memcpy(g_mpool + g_mpool_n, text, size_t(len));
    g_mpool[g_mpool_n + uint32_t(len)] = 0;
    g_mpool_n += uint32_t(len) + 1;
}
/* xglFontPrint (any caller): the stream's length by the font engine's code sizes (arguments
   may hold zero bytes), then the same capture as the menu text. */
extern "C" int xsg_ui_menu_text(int x, int y, int param, const unsigned char* text, int len);
extern "C" unsigned char xglFontGetSPcodeSize(unsigned char code, unsigned char* text);
extern "C" int xglFontGetStringWidth(const char* text);
extern "C" int xsg_ui_font_text(int x, int y, int color, const char* text) {
    if (!xsg_ui_rebuilt() || !text) return 0;
    const unsigned char* t = reinterpret_cast<const unsigned char*>(text);
    int i = 0;
    while (t[i] && i < 4096) {
        if (t[i] < 0x20) i += 1 + std::max(0, int(xglFontGetSPcodeSize(t[i], const_cast<unsigned char*>(t + i))));
        else i += t[i] >= 0x80 ? 2 : 1;
    }
    g_last_text_op = -1;
    g_last_text_frame = xsg_frame;
    g_last_x = x; g_last_y = y; g_last_param = color;
    const int r = xsg_ui_menu_text(x, y, color, t, i);
    // the game's own width for the string (the font engine's metrics: its scale, monospace or
    // proportional), so xsg-gpu can match the original proportions
    
    if (r && g_last_text_op >= 0) g_mops[g_last_text_op].dw = int16_t(std::min(2047, xglFontGetStringWidth(text)));
    return r || i == 0;
}
/* xglFontPrintDirect after an xglFontPrint (subtitles: a style prefix, then the line): the text
   joins that record.  Without one captured this frame it goes to the GS as before. */
extern "C" int xsg_ui_font_append(const char* text) {
    if (!xsg_ui_rebuilt() || !text || g_last_text_frame != xsg_frame) return 0;
    // the prefix's record (style codes only) is replaced by one at its position with the text
    if (g_last_text_op >= 0 && g_last_text_op < int(g_mop_n) && g_mops[g_last_text_op].type == GPU_KG_TEXT)
        g_mops[g_last_text_op].type = 0;
    const unsigned char* t = reinterpret_cast<const unsigned char*>(text);
    int i = 0;
    while (t[i] && i < 4096) {
        if (t[i] < 0x20) i += 1 + std::max(0, int(xglFontGetSPcodeSize(t[i], const_cast<unsigned char*>(t + i))));
        else i += t[i] >= 0x80 ? 2 : 1;
    }
    xsg_ui_menu_text(g_last_x, g_last_y, g_last_param, t, i);
    return 1;
}

/* The title screen draws its choices from baked text images; the rebuilt UI gets the state.
   Its frame has no font flush, so it is published here. */
extern "C" void xsg_ui_menu_end(void);
extern "C" void xsg_ui_title(int sel, int hdd, int fade) {
    g_mop_n = 0;
    g_mpool_n = 0;
    const int op = xsg_ui_mop(GPU_KG_TITLE, 0, 0, 0, 0, 0xffffff, uint32_t(fade), hdd);
    if (op >= 0) g_mops[op].a = sel;
    xsg_ui_menu_end();
}

/* The end of the frame's font flush: publish whatever was captured (possibly nothing). */
extern "C" void xsg_ui_menu_end(void);
extern "C" void xsg_ui_frame_end(void) {
    // an empty flush does not replace a list published earlier in the same frame (the title)
    if (xsg_ui_rebuilt() && (g_mop_n || g_published_frame != xsg_frame)) xsg_ui_menu_end();
}
extern "C" void xsg_ui_menu_end(void) {
    if (xsggpu::shm) {
        GpuUiMenuList* m = reinterpret_cast<GpuUiMenuList*>(reinterpret_cast<uint8_t*>(xsggpu::shm) + XSG_GPU_UI_MENU_OFF);
        const uint32_t s = m->seq;
        __atomic_store_n(&m->seq, s + 1, __ATOMIC_RELAXED);
        __atomic_thread_fence(__ATOMIC_RELEASE);
        std::memcpy(m->op, g_mops, sizeof(GpuUiKgOp) * g_mop_n);
        std::memcpy(m->pool, g_mpool, g_mpool_n);
        m->nops = g_mop_n;
        m->pool_used = g_mpool_n;
        m->frame = xsg_frame;
        __atomic_store_n(&m->seq, s + 2, __ATOMIC_RELEASE);
    }
    if (g_mop_n) g_published_frame = xsg_frame;
    g_mop_n = 0;
    g_mpool_n = 0;
    g_menu_last = xsg_frame;
}

/* eMessageDrawType01's stream for one message box: in a captured menu (a list flushed in
   the last frames), normalised for xsg-gpu and kept from the GS.  Normal form: text (ASCII
   and EUC pairs), '\n', 0x01 r g b (colour). */
extern "C" unsigned char xglFontGetSPcodeSize(unsigned char code, unsigned char* text);
extern "C" int xsg_ui_rebuilt(void);
extern "C" int xsg_ui_menu_text(int x, int y, int param, const unsigned char* text, int len) {
    if (!xsg_ui_rebuilt() || len <= 0) return 0;
    char out[1024];
    int n = 0;
    // inline icons: "19 00 c0 c1 19 ctl" placeholders, msg_spr slots in order, ending at msg_spr_count
    extern int msg_spr_count;
    int icons = 0;
    for (int i = 0; i + 5 < len; ++i)
        if (text[i] == 0x19 && text[i + 1] == 0 && text[i + 4] == 0x19) icons++, i += 5;
    int slot = msg_spr_count - icons;
    for (int i = 0; i < len && n < int(sizeof out) - 8;) {
        const unsigned char c = text[i];
        if (c == 0x19 && i + 5 < len && text[i + 1] == 0 && text[i + 4] == 0x19) {
            out[n++] = 2;
            out[n++] = char(1 + std::max(0, slot++));   // never 0 (the pool is NUL-terminated)
            i += 6;
            continue;
        }
        if (c >= 0x80) {
            out[n++] = char(c);
            if (i + 1 < len) out[n++] = char(text[i + 1]);
            i += 2;
        } else if (c >= 0x20) {
            out[n++] = char(c);
            i++;
        } else {
            const int args = xglFontGetSPcodeSize(c, const_cast<unsigned char*>(text + i));
            if (c >= 1 && c <= 5 && i + 1 < len) {   // set x / y: 0x04 / 0x05 + an offset from the origin (+0x4000)
                const int v = ((c << 8) | text[i + 1]) - (c <= 3 ? 0x100 : 0x400);
                const int d = (c <= 3 ? v - x : v - y) + 0x4000;
                out[n++] = char(c <= 3 ? 4 : 5);
                out[n++] = char((d >> 7) | 0x80);
                out[n++] = char((d & 0x7f) | 0x80);
            } else if (c == 0x0a) out[n++] = '\n';
            else if (c == 0x09) out[n++] = '\t';
            else if (c == 0x0c && i + 3 < len) {
                out[n++] = 1;
                out[n++] = char(text[i + 1] | 1); out[n++] = char(text[i + 2] | 1); out[n++] = char(text[i + 3] | 1);
            }
            i += 1 + (args < 0 ? 0 : args);
        }
    }
    const int op = xsg_ui_mop(GPU_KG_TEXT, x, y, 0, 0, param & 0xffffff, 0x80808080u, 0);
    xsg_ui_mop_text(op, out, n);
    g_last_text_op = op;
    return 1;
}

/* The native renderer keeps the GS-memory mirror the menu art is snapshotted from. */
extern "C" int xsg_native_on(void);
extern "C" int xsg_ui_mirror_on(void) { return xsg_native_on(); }

/* A texture directory (xtx: +8 count, +0xC first entry; entries {DBW << 16 | RRW, RRH,
   VRAM word offset from 0xF0000, qwords, data offset}, uploaded as PSMCT32): its key, sent
   to xsg-gpu when its content (sampled) is new. */
extern "C" unsigned xsg_ui_texset(const void* directory) {
    if (!directory || !xsggpu::shm) return 0;
    const uint8_t* d = static_cast<const uint8_t*>(directory);
    int32_t n, first;
    std::memcpy(&n, d + 8, 4);
    std::memcpy(&first, d + 12, 4);
    if (n <= 0 || n > 64 || first <= 0 || first > 4096) return 0;
    struct E { uint32_t pos, size; int32_t addr, count, offset; };
    const E* e = reinterpret_cast<const E*>(d + first);
    uint64_t h = 1469598103934665603ull;
    size_t words = 0;
    for (int i = 0; i < n; ++i) {
        const uint32_t w = e[i].pos & 0xffff, hh = e[i].size;
        const size_t nw = size_t(w) * hh;
        const uint32_t* px = reinterpret_cast<const uint32_t*>(d + e[i].offset);
        // the data may start with its own GIF IMAGE tag
        if (uint32_t(e[i].count) == nw / 4 + 1) px += 4;
        // every word: the game patches palettes inside the loaded texture (portraits)
        const uint64_t* q = reinterpret_cast<const uint64_t*>(px);
        for (size_t k = 0; k < nw / 2; ++k) h = (h ^ q[k]) * 1099511628211ull;
        h = (h ^ (uint64_t(e[i].addr) << 20 ^ e[i].pos)) * 1099511628211ull;
        words += nw;
    }
    const unsigned key = unsigned(h ^ (h >> 32)) | 1u;
    static unsigned sent[8];
    for (unsigned k : sent)
        if (k == key) return key;
    static int next;
    sent[next++ & 7] = key;
    std::vector<uint8_t> buf(sizeof(GpuUiTexEntry) * size_t(n) + words * 4);
    GpuUiTexEntry* te = reinterpret_cast<GpuUiTexEntry*>(buf.data());
    uint8_t* out = buf.data() + sizeof(GpuUiTexEntry) * size_t(n);
    for (int i = 0; i < n; ++i) {
        const uint32_t w = e[i].pos & 0xffff, hh = e[i].size;
        const size_t nw = size_t(w) * hh;
        const uint32_t* px = reinterpret_cast<const uint32_t*>(d + e[i].offset);
        if (uint32_t(e[i].count) == nw / 4 + 1) px += 4;
        te[i] = {uint32_t(e[i].addr + 0xF0000) >> 6, e[i].pos >> 16, w, hh, uint32_t(nw)};
        std::memcpy(out, px, nw * 4);
        out += nw * 4;
    }
    xsggpu::Send(GPU_MSG_UI_TEX, key, uint32_t(n), buf.data(), uint32_t(buf.size()));
    xsg_logf("[ui] texture set %08x: %d uploads, %zu KB\n", key, n, words * 4 / 1024);
    return key;
}

/* A whole picture for the rebuilt UI (the title screen's): sent once, upscaled by xsg-gpu. */
extern "C" void xsg_ui_image(int id, const void* rgba, int w, int h) {
    if (!xsggpu::shm || !rgba || w <= 0 || h <= 0) return;
    xsggpu::Send(GPU_MSG_UI_IMAGE, uint32_t(id), uint32_t(w) | uint32_t(h) << 16, rgba, uint32_t(w * h * 4));
}

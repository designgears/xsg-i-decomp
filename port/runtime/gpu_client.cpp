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
#include "gpu_ipc.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

    const char* bin_env = std::getenv("XSG_GPU_BIN");
    std::string bin = bin_env && *bin_env ? bin_env : DefaultBin();
    std::vector<std::string> env_s;
    for (char** e = environ; *e; ++e)
        if (std::strncmp(*e, "XSG_GPU_STREAM=", 15) != 0) env_s.push_back(*e);
    if (stream) env_s.push_back(std::string("XSG_GPU_STREAM=") + stream);
    std::vector<char*> envp;
    for (auto& e : env_s) envp.push_back(&e[0]);
    envp.push_back(nullptr);
    char* argv[] = {&bin[0], nullptr};
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fd, XSG_GPU_FD);
    pid_t pid;
    int rc = posix_spawn(&pid, bin.c_str(), &fa, nullptr, argv, envp.data());
    posix_spawn_file_actions_destroy(&fa);
    close(fd);
    if (rc != 0) {
        xsg_logf("[gpu] cannot start %s: %s (XSG_GPU_BIN overrides the path)\n", bin.c_str(), std::strerror(rc));
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

}  // namespace xsggpu

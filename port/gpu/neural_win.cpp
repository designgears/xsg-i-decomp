/*
 * Windows: DLSS 5 Neural Rendering through the DLSS5VKLayer helper (neural_win.h).
 *
 * The exchange, as the DLSS5VKLayer Linux layer does it:
 *   file mapping = 64 KiB header | the frame for the model | the model's answer
 *   client: pixels -> region 1, width/height, a release fence, seq_req + 1
 *   helper: runs the model, answer -> region 2, answeredW/H, seq_ok, seq_resp = seq_req
 * Only the fields below are touched.  Their offsets are those of DLSS5VKLayer's shm protocol
 * version 20 (magic 'GNR2'); a helper of another version re-initialises the header and the
 * version check below then refuses it, so frames pass through unchanged.
 */
#ifdef _WIN32
#include "neural_win.h"

#include <windows.h>
#include <winioctl.h>   // FSCTL_SET_SPARSE (WIN32_LEAN_AND_MEAN leaves it out)

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "menu/cfg.h"
#include "xsg_win_compat.h"

namespace xsgneural {
namespace {

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[neural] %s\n", b);
}

/* DLSS5VKLayer shm protocol v20: sizes and the header offsets this client uses. */
constexpr uint32_t kMagic = 0x32524E47u;   // 'GNR2'
constexpr uint32_t kVersion = 20;
constexpr size_t kHeaderBytes = 65536;
constexpr size_t kMaxFrame = size_t(7680) * 4320 * 8;
constexpr size_t kTotal = kHeaderBytes + 2 * kMaxFrame;
constexpr uint32_t kMinW = 64, kMinH = 64, kMaxW = 7680, kMaxH = 4320;
enum : size_t {
    kOffMagic = 0, kOffVersion = 4, kOffSeqReq = 8, kOffSeqResp = 12, kOffWidth = 16, kOffHeight = 20,
    kOffFormat = 24, kOffQuit = 28, kOffHeartbeat = 32, kOffControlSeq = 36, kOffEnabled = 44,
    kOffHelperState = 176, kOffLayerPid = 216, kOffLayerWidth = 228, kOffLayerHeight = 232,
    kOffLayerHeartbeat = 252, kOffGameNameSeq = 648, kOffGameName = 652, kOffSeqOk = 1872,
    kOffAnsweredW = 1884, kOffAnsweredH = 1888, kOffHdrEncode = 1948,
};
constexpr uint32_t kHelperStopped = 5;
constexpr size_t kNameBytes = 128;

HANDLE g_file = INVALID_HANDLE_VALUE, g_map = nullptr, g_proc = nullptr;
uint8_t* g_base = nullptr;
bool g_started = false, g_dead = false, g_answered = false, g_on = true;
int g_timeouts = 0;
double g_retry_ms = 0;

std::atomic<uint32_t>& U32(size_t off) { return *reinterpret_cast<std::atomic<uint32_t>*>(g_base + off); }

double NowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string ExeDir() {
    char p[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s(p, n);
    const size_t sl = s.find_last_of("\\/");
    return sl == std::string::npos ? "." : s.substr(0, sl);
}

bool Exists(const std::string& p) { return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

/* [renderer] neural_helper, else dlssnr\dlssnr_helper.exe next to xsg-gpu.exe or in %LOCALAPPDATA%\xsg. */
std::string HelperPath() {
    const char* c = xsgcfg::CStr("renderer.neural_helper");
    if (c && *c) return c;
    const std::string a = ExeDir() + "\\dlssnr\\dlssnr_helper.exe";
    if (Exists(a)) return a;
    std::string la = XsgWinDir("LOCALAPPDATA");
    for (char& ch : la) if (ch == '/') ch = '\\';
    const std::string b = la + "\\xsg\\dlssnr\\dlssnr_helper.exe";
    return Exists(b) ? b : a;
}

bool Header() {
    return U32(kOffMagic).load() == kMagic && U32(kOffVersion).load() == kVersion;
}

}  // namespace

bool Start() {
    if (g_started) return g_base != nullptr;
    g_started = true;
    const std::string helper = HelperPath();
    if (!Exists(helper)) {
        Log("DLSS 5: the helper is not installed (%s); set [renderer] neural_helper. Frames pass through.",
            helper.c_str());
        return false;
    }
    // The exchange file: the helper opens the same path (DLSSNR_SHM).
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    const std::string dir = std::string(tmp) + "xsg-dlssnr";
    CreateDirectoryA(dir.c_str(), nullptr);
    const std::string shm = dir + "\\shm.bin";
    g_file = CreateFileA(shm.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (g_file == INVALID_HANDLE_VALUE) { Log("DLSS 5: cannot open %s (%lu)", shm.c_str(), GetLastError()); return false; }
    // Sparse, as on Linux: the frame regions are sized for 8K float16 but an SDR session writes a
    // fraction of one.
    DWORD ret = 0;
    DeviceIoControl(g_file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &ret, nullptr);
    LARGE_INTEGER size;
    size.QuadPart = LONGLONG(kTotal);
    SetFilePointerEx(g_file, size, nullptr, FILE_BEGIN);
    SetEndOfFile(g_file);
    g_map = CreateFileMappingA(g_file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    g_base = g_map ? static_cast<uint8_t*>(MapViewOfFile(g_map, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0)) : nullptr;
    if (!g_base) { Log("DLSS 5: cannot map %s (%lu)", shm.c_str(), GetLastError()); return false; }

    // The helper, in this process's job object (it ends with xsg-gpu), its output in the log folder.
    std::string env;
    for (const char* e = GetEnvironmentStringsA(); e && *e; e += std::strlen(e) + 1)
        if (std::strncmp(e, "DLSSNR_SHM=", 11) && std::strncmp(e, "DLSSNR_BIN_DIR=", 15) && std::strncmp(e, "DLSSNR_LOG=", 11))
            env.append(e).push_back('\0');
    env.append("DLSSNR_SHM=" + shm).push_back('\0');
    if (const char* b = xsgcfg::CStr("renderer.neural_bins"); b && *b) env.append(std::string("DLSSNR_BIN_DIR=") + b).push_back('\0');
    env.push_back('\0');
    std::string logdir = XsgWinDir("APPDATA");
    for (char& ch : logdir) if (ch == '/') ch = '\\';
    const std::string logp = logdir + "\\xsg\\dlssnr_helper.log";
    env.pop_back();   // reopen the block: the helper writes its own log through DLSSNR_LOG
    env.append("DLSSNR_LOG=" + logp).push_back('\0');
    env.push_back('\0');
    SECURITY_ATTRIBUTES sa = {sizeof sa, nullptr, TRUE};
    const std::string outp = logdir + "\\xsg\\dlssnr_helper_out.log";
    HANDLE lf = CreateFileA(outp.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOA si = {};
    si.cb = sizeof si;
    if (lf != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = lf;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    PROCESS_INFORMATION pi = {};
    std::string cmdline = "\"" + helper + "\"";
    const std::string hdir = helper.substr(0, helper.find_last_of("\\/"));
    if (!CreateProcessA(helper.c_str(), cmdline.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, env.data(),
                        hdir.c_str(), &si, &pi)) {
        Log("DLSS 5: cannot start %s (%lu)", helper.c_str(), GetLastError());
        if (lf != INVALID_HANDLE_VALUE) CloseHandle(lf);
        return false;
    }
    if (lf != INVALID_HANDLE_VALUE) CloseHandle(lf);
    CloseHandle(pi.hThread);
    g_proc = pi.hProcess;
    // The helper initialises the header (or keeps a current one); wait for it briefly.
    const double t0 = NowMs();
    while (!Header() && NowMs() - t0 < 5000.0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!Header()) {
        Log("DLSS 5: the helper did not set up %s (version %u, want %u); frames pass through. Log: %s", shm.c_str(),
            U32(kOffVersion).load(), kVersion, logp.c_str());
        g_dead = true;
        return true;
    }
    static const char kName[] = "Xenosaga Episode I";
    std::memcpy(g_base + kOffGameName, kName, sizeof kName);
    U32(kOffGameNameSeq).fetch_add(1);
    U32(kOffLayerPid).store(uint32_t(GetCurrentProcessId()));
    U32(kOffEnabled).store(g_on ? 1u : 0u);
    U32(kOffControlSeq).fetch_add(1);
    Log("DLSS 5 Neural Rendering: helper %s started (pid %lu), exchange %s, helper log %s", helper.c_str(),
        pi.dwProcessId, shm.c_str(), logp.c_str());
    return true;
}

bool Active() {
    if (!g_base || !g_on) return false;
    if (g_dead && NowMs() >= g_retry_ms && Header() && U32(kOffHelperState).load() != kHelperStopped) {
        g_dead = false;
        g_timeouts = 0;
    }
    return !g_dead;
}

void SetEnabled(bool on) {
    g_on = on;
    if (g_base && Header()) {
        U32(kOffEnabled).store(on ? 1u : 0u);
        U32(kOffControlSeq).fetch_add(1);
    }
}

bool Process(const uint8_t* in, uint32_t w, uint32_t h, uint8_t* out) {
    if (!Active() || !Header()) return false;
    if (w < kMinW || h < kMinH || w > kMaxW || h > kMaxH) return false;
    if (U32(kOffQuit).load()) { g_dead = true; return false; }
    const size_t bytes = size_t(w) * h * 4;
    uint8_t* const pin = g_base + kHeaderBytes;
    const uint8_t* const pout = pin + kMaxFrame;
    std::memcpy(pin, in, bytes);
    U32(kOffWidth).store(w);
    U32(kOffHeight).store(h);
    U32(kOffFormat).store(1u);
    U32(kOffHdrEncode).store(0u);
    U32(kOffLayerWidth).store(w);
    U32(kOffLayerHeight).store(h);
    U32(kOffLayerHeartbeat).fetch_add(1);
    const uint32_t req = U32(kOffSeqReq).load() + 1;
    std::atomic_thread_fence(std::memory_order_release);
    U32(kOffSeqReq).store(req);
    // The first frame at a size builds the model's feature (hundreds of ms); after that it is ms.
    const bool present = U32(kOffHelperState).load() != kHelperStopped;
    const double budget = !present ? 20.0 : (g_answered ? 1000.0 : 10000.0);
    const double t0 = NowMs();
    for (;;) {
        if (U32(kOffSeqResp).load() >= req) {
            std::atomic_thread_fence(std::memory_order_acquire);
            g_timeouts = 0;
            g_answered = true;
            const bool ok = U32(kOffSeqOk).load() >= req && U32(kOffAnsweredW).load() == w &&
                            U32(kOffAnsweredH).load() == h;
            if (ok) std::memcpy(out, pout, bytes);
            return ok;
        }
        if (U32(kOffQuit).load()) { g_dead = true; return false; }
        const double el = NowMs() - t0;
        if (el >= budget) break;
        if (el < 2.0) std::this_thread::yield();
        else std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    if (++g_timeouts >= 4) {
        g_dead = true;
        g_retry_ms = NowMs() + 5000.0;
        Log("DLSS 5: no answer from the helper in %.0f ms x4; showing the game's frames, retrying in 5 s", budget);
    }
    return false;
}

void Stop() {
    if (g_base && Header()) U32(kOffQuit).store(1u);
    if (g_proc) {
        WaitForSingleObject(g_proc, 2000);
        TerminateProcess(g_proc, 0);
        CloseHandle(g_proc);
        g_proc = nullptr;
    }
}

}  // namespace xsgneural
#endif

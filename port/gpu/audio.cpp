/* xsg-gpu: the game's audio, played through SDL3 (audio.h, gpu_ipc.h GpuAudio).
 *
 * The game's sound driver renders the SPU2 mix into a shared ring and steers how much
 * it renders by the ring's fill (iop_sound.c); this side opens an SDL3 playback stream
 * (PipeWire / PulseAudio / ALSA, whatever SDL picks) and pulls from the ring in SDL's
 * own audio thread, at the device's pace: the render loop can stall without the sound
 * stalling.  When the game itself stalls long enough for the ring to run dry, the last
 * sample decays to silence (~1 ms) instead of a hard cut, and the sound ramps back in
 * over 2 ms when it returns. */
#include "audio.h"

#include "gpu_ipc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef XSG_GPU_SDL
#include <SDL3/SDL.h>
#endif

namespace xsgaudio {
namespace {
GpuAudio* g_ring;
#ifdef XSG_GPU_SDL
SDL_AudioStream* g_stream;
bool g_tried;

// device-thread state
float g_hold[2];          // the last sample played (decays while starved)
int g_ramp;               // frames of fade-in left after a gap
bool g_starved;
constexpr int kRamp = 96; // 2 ms

void Pull(void*, SDL_AudioStream* stream, int additional, int) {
    if (additional <= 0) return;
    const uint32_t want = uint32_t(additional) / 4;
    static std::vector<int16_t> buf;
    buf.resize(size_t(want) * 2);
    const uint32_t r = g_ring->rpos, w = __atomic_load_n(&g_ring->wpos, __ATOMIC_ACQUIRE);
    const uint32_t have = std::min(want, w - r);
    for (uint32_t i = 0; i < have;) {
        const uint32_t at = (r + i) & (XSG_GPU_AUDIO_FRAMES - 1), k = std::min(have - i, XSG_GPU_AUDIO_FRAMES - at);
        std::memcpy(buf.data() + size_t(i) * 2, const_cast<const int16_t*>(g_ring->pcm) + size_t(at) * 2, size_t(k) * 4);
        i += k;
    }
    __atomic_store_n(&g_ring->rpos, r + have, __ATOMIC_RELEASE);
    if (have && g_starved) { g_starved = false; g_ramp = kRamp; }
    for (uint32_t i = 0; i < have && g_ramp > 0; ++i, --g_ramp) {   // back from a gap: fade in
        const float g = float(kRamp - g_ramp) / kRamp;
        buf[2 * i] = int16_t(buf[2 * i] * g);
        buf[2 * i + 1] = int16_t(buf[2 * i + 1] * g);
    }
    if (have) { g_hold[0] = buf[2 * (have - 1)]; g_hold[1] = buf[2 * (have - 1) + 1]; }
    if (have < want) {   // starved: the last sample decays to silence
        if (!g_starved) { g_starved = true; g_ring->underruns = g_ring->underruns + 1; }
        for (uint32_t i = have; i < want; ++i) {
            g_hold[0] *= 0.98f;
            g_hold[1] *= 0.98f;
            buf[2 * i] = int16_t(g_hold[0]);
            buf[2 * i + 1] = int16_t(g_hold[1]);
        }
    }
    const uint64_t played = (uint64_t(g_ring->played_hi) << 32 | g_ring->played_lo) + want;
    g_ring->played_lo = uint32_t(played);
    g_ring->played_hi = uint32_t(played >> 32);
    SDL_PutAudioStreamData(stream, buf.data(), int(want * 4));
}
#endif
}  // namespace

void Init(void* shm_base) {
    g_ring = reinterpret_cast<GpuAudio*>(static_cast<uint8_t*>(shm_base) + XSG_GPU_AUDIO_OFF);
}

void Poll() {
#ifdef XSG_GPU_SDL
    if (g_stream || g_tried || !g_ring || !__atomic_load_n(&g_ring->on, __ATOMIC_ACQUIRE)) return;
    g_tried = true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "[audio] SDL audio: %s (no sound)\n", SDL_GetError());
        return;
    }
    // a short device buffer: the ring in front of it already absorbs the game's jitter
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
    const SDL_AudioSpec spec = {SDL_AUDIO_S16LE, 2, int(g_ring->rate ? g_ring->rate : 48000)};
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Pull, nullptr);
    if (!g_stream) {
        std::fprintf(stderr, "[audio] SDL_OpenAudioDeviceStream: %s (no sound)\n", SDL_GetError());
        return;
    }
    // start from what is queued now, not from a ring filled while no device was open
    __atomic_store_n(&g_ring->rpos, __atomic_load_n(&g_ring->wpos, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
    SDL_ResumeAudioStreamDevice(g_stream);
    std::fprintf(stderr, "[audio] SDL3 playback on \"%s\" (%s), %d Hz stereo\n",
                 SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(g_stream)) ?: "default", SDL_GetCurrentAudioDriver() ?: "?",
                 spec.freq);
#endif
}

uint32_t Underruns() { return g_ring ? g_ring->underruns : 0; }

}  // namespace xsgaudio

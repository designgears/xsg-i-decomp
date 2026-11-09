/* xsg-gpu: plays the game's audio ring (gpu_ipc.h GpuAudio) through SDL3.  See audio.cpp. */
#pragma once
#include <cstdint>

namespace xsgaudio {
/* The shared memory's base (the ring sits at XSG_GPU_AUDIO_OFF). */
void Init(void* shm_base);
/* Once per vsync: opens the device when the game starts writing. */
void Poll();
/* Times the ring ran dry (the game stalled past the buffer). */
uint32_t Underruns();
}  // namespace xsgaudio

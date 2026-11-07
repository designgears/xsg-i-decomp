/* Stage 9: the game side of the xsg-gpu ring (gpu_client.cpp, gpu_ipc.h).
   Called with gs_bridge.cpp's lock held; single producer. */
#ifndef XSG_GPU_CLIENT_H
#define XSG_GPU_CLIENT_H

#include <cstdint>

namespace xsggpu {

/* Spawns xsg-gpu and waits until its Vulkan device is up. `stream` is the
   XSG_STREAM FIFO the renderer should feed itself (nullptr: none). */
bool Start(const char* stream);
bool Active();

void Gif(int path, std::uint64_t lo, std::uint64_t hi);
void GifBlock(int path, const void* qwords, std::uint32_t n);
/* The GS privileged registers (game memory at 0x12000000/0x12001000):
   every changed one is forwarded.  Called before each vsync and readback. */
void SyncPriv();
void Vsync(std::uint32_t frame, std::uint32_t field);
/* Synchronous: all 4 MB of the renderer's local memory, after everything
   sent so far.  The pointer stays valid until the next call. */
const std::uint8_t* ReadVram();
/* Synchronous: `qwords` of local->host transfer data (TRXDIR 1). */
void ReadFifo(void* dst, std::uint32_t qwords);
void Flush();

}  // namespace xsggpu

#endif

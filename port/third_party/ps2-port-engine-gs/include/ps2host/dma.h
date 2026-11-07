/* Shim for the xsg port: only the trace variable the GS decoder reads. */
#ifndef PS2HOST_DMA_H
#define PS2HOST_DMA_H
#include <cstdint>
namespace ps2host { extern std::uint32_t g_dma_source; }
#endif

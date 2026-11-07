/* Shim for the xsg port: upstream's profiler is not vendored; Timed is a no-op. */
#ifndef PS2HOST_PROFILE_H
#define PS2HOST_PROFILE_H
namespace ps2host {
enum class Section { GsDraw, GsUpload, Vu1, Vu0, GpuBridge, GpuRender, GpuSync, Dma, Iop, Spu2, GsWait, Count };
class Timed { public: explicit Timed(Section) {} Timed(const Timed&) = delete; Timed& operator=(const Timed&) = delete; };
}
#endif

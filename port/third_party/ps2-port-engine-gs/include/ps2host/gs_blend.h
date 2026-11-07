/* Shim for the xsg port: only GsPicture (Gs::Keep's sink) from upstream gs_blend.h. */
#ifndef PS2HOST_GS_BLEND_H
#define PS2HOST_GS_BLEND_H
#include <cstdint>
#include <vector>
namespace ps2host {
struct GsPicture {
    struct Entry { char kind; std::uint8_t path; std::uint32_t address; std::uint64_t value; std::uint32_t first, count; };
    std::vector<Entry> entries;
    std::vector<std::uint32_t> paths[3];
};
}
#endif

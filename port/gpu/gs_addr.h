/* GS memory addresses of frame and Z buffer pixels, by table (#3).
 *
 * Memory::Address computes the whole page/block/column swizzle per call; the
 * live pass called it for every pixel of the displayed buffer, its Z and the
 * ownership plane (~2 ms a frame on the renderer thread).  A frame or Z
 * buffer starts on a page (FBP/ZBP count pages), so a pixel's address is its
 * page's base plus an in-page offset that depends only on the format and the
 * pixel's place in the page (64 x 32 pixels, 64 x 64 for 16-bit formats).
 * The offsets come from Memory::Address itself, once per format. */
#pragma once
#include <cstdint>
#include "ps2host/gs.h"

namespace xsggs {

struct PageTab {
    uint32_t ph = 0;          // page height (pixels); 0: not a format this covers
    uint32_t off[64 * 64];
};

inline const PageTab& Tab(uint32_t psm) {
    static PageTab tabs[64];
    static bool ready = [] {
        static const uint32_t fmts[] = {0x00, 0x01, 0x02, 0x0A, 0x30, 0x31, 0x32, 0x3A};
        for (uint32_t f : fmts) {
            PageTab& t = tabs[f];
            t.ph = (f == 0x02 || f == 0x0A || f == 0x32 || f == 0x3A) ? 64 : 32;
            for (uint32_t y = 0; y < t.ph; ++y)
                for (uint32_t x = 0; x < 64; ++x) {
                    uint32_t nib = 0;
                    t.off[y * 64 + x] = ps2host::gs::Memory::Address(0, 1, x, y, f, &nib);
                }
        }
        return true;
    }();
    (void)ready;
    return tabs[psm & 63];
}

/* Byte address (within the 4 MB) of pixel (x, y) of a buffer at page `pages` (FBP/ZBP). */
inline uint32_t Addr(uint32_t pages, uint32_t fbw, uint32_t x, uint32_t y, uint32_t psm) {
    const PageTab& t = Tab(psm);
    if (t.ph && fbw && psm < 64) {
        const uint32_t page = pages + (y / t.ph) * fbw + (x >> 6);
        return (page * 8192u + t.off[(y % t.ph) * 64 + (x & 63)]) & 0x3fffff;
    }
    uint32_t nib = 0;
    return ps2host::gs::Memory::Address(pages * 32, fbw, x, y, psm, &nib) & 0x3fffff;
}

}  // namespace xsggs

/*
 * Windows port (port/docs/WINDOWS.md): xsg.exe, the stub that reserves PS2 RAM.
 *
 * The game (decompiled code that uses PS2 addresses as host pointers) needs EE
 * RAM at 0x10000-0x2000000.  In a Windows process the heap, the main stack and
 * the NLS tables are put at low addresses before any of the program's code
 * runs; only the main executable's image is mapped before them.  So this
 * executable is linked at image base 0x10000 with a zero-filled (bss) array
 * that makes its image span the whole range, has no CRT and no relocations,
 * and imports xsg.dll (the game and its runtime, linked at 0x40000000).  Its
 * entry point calls xsg_win32_start (win32/platform_win32.c), which never
 * returns: it unmaps this image and maps the EE RAM section (and its
 * 0x20000000 / 0x30000000 aliases) in its place.
 *
 * Built with -nostdlib -e _stub_entry@0 --image-base 0x10000 (CMakeLists.txt).
 */
#define XSG_STUB_RESERVE (0x02000000 - 0x00013000)   /* image base + headers, .text, .rdata */

char xsg_ps2ram_reserve[XSG_STUB_RESERVE];

__declspec(dllimport) void xsg_win32_start(void *reserve_lo, void *reserve_hi);

void __stdcall stub_entry(void)
{
    xsg_win32_start(xsg_ps2ram_reserve, xsg_ps2ram_reserve + sizeof xsg_ps2ram_reserve);
    for (;;) {
    }
}

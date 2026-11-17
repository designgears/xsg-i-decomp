#!/bin/sh
# usr/bin/gpu/xsg-gpu in the AppImage: the game spawns this path for its
# renderer (gpu_client.cpp DefaultBin: <dir of /proc/self/exe>/gpu/xsg-gpu).
#
# xsg-gpu loads host libraries into its own process: libvulkan, the Vulkan
# driver, and whatever SDL3 dlopens (Wayland/X11, PipeWire/Pulse).  Those must
# run on a glibc (and libstdc++) at least as new as the one they were built
# against, so the newer of host and bundled wins:
#   - glibc: the host's when it is >= the bundled one, else the bundled
#     loader + libc set in lib64/glibc;
#   - libstdc++/libgcc_s: the bundled pair (lib64/cxx) unless the host's
#     libstdc++ is newer;
#   - SDL3 always bundled (lib64).
# --library-path instead of LD_LIBRARY_PATH, so nothing leaks into the
# processes xsg-gpu starts.  Vulkan itself is never bundled.
# XSGPKG_GLIBC=host|bundled forces the glibc choice (for testing).
L=$(cd "$(dirname "$(readlink -f "$0")")/../../lib64" && pwd)
ver_ge() { [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | head -n1)" = "$2" ]; }

bundled_glibc=$(cat "$L/glibc/VERSION" 2>/dev/null)
host_glibc=$(getconf GNU_LIBC_VERSION 2>/dev/null | sed 's/^glibc //')
mode=${XSGPKG_GLIBC:-}
if [ -z "$mode" ]; then
    if [ -n "$host_glibc" ] && [ -e /lib64/ld-linux-x86-64.so.2 ] && ver_ge "$host_glibc" "$bundled_glibc"; then
        mode=host
    else
        mode=bundled
    fi
fi

path=$L
cxx=bundled
host_cxx=$(ldconfig -p 2>/dev/null | sed -n 's/^[[:space:]]*libstdc++\.so\.6 (libc6,x86-64) => //p' | head -n1)
if [ -n "$host_cxx" ] && [ -r "$host_cxx" ]; then
    hv=$(grep -aoE 'GLIBCXX_3\.4\.[0-9]+' "$host_cxx" | sort -Vu | tail -n1)
    bv=$(cat "$L/cxx/VERSION" 2>/dev/null)
    [ -n "$hv" ] && [ "$hv" != "$bv" ] && ver_ge "$hv" "$bv" && cxx=host
fi
[ "$cxx" = bundled ] && path=$path:$L/cxx

if [ "$mode" = host ]; then
    loader=/lib64/ld-linux-x86-64.so.2
else
    loader=$L/glibc/ld-linux-x86-64.so.2
    path=$L/glibc:$path
fi
[ -n "$XSGPKG_DEBUG" ] && echo "[appimage] xsg-gpu: glibc $mode (host ${host_glibc:-?}, bundled $bundled_glibc), libstdc++ $cxx" >&2
: "${SDL_APP_ID:=xsg-gpu}"
export SDL_APP_ID
exec "$loader" --library-path "$path" --argv0 xsg-gpu "$L/../libexec/xsg/xsg-gpu" "$@"

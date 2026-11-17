#!/usr/bin/env bash
# Package the PC port as one AppImage: Xenosaga_Episode_I-x86_64.AppImage.
# See port/docs/PACKAGING.md.
#
#   port/packaging/make_appimage.sh [--build] [--out FILE] [--keep-appdir]
#
#   --build        build port/build first (cmake --build, under the build lock)
#   --out FILE     output path (default: <repo>/Xenosaga_Episode_I-x86_64.AppImage)
#   --keep-appdir  leave the AppDir in .work/appimage/AppDir afterwards
#
# Env: XSG_BUILD (default port/build), APPIMAGETOOL (default: pinned download
# in .work/tools/appimage), XSG_GAME_DIR (default <repo>/game: its uncensored/
# folder is packed as uncensored/ at the AppImage root, see PACKAGING.md).
set -euo pipefail

cat >&2 <<'EOF'
==============================================================================
 Xenosaga Episode I PC port -- AppImage packager

 The binaries packed here (xsg, xsg-gpu) hold the decompiled code and the
 layout of the game's data, but none of the game's bytes: the game reads its
 data from your own ISO at start-up ([game] iso in settings.ini, or the
 first-run setup screen).  If you pack uncensored/3039C_un.pss, that file is
 game data: keep such a package to yourself.
==============================================================================
EOF

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd "$here/../.." && pwd)
build=${XSG_BUILD:-$repo/port/build}
out=$repo/Xenosaga_Episode_I-x86_64.AppImage
keep=0 do_build=0
while [ $# -gt 0 ]; do
    case $1 in
        --build) do_build=1 ;;
        --out) out=$(realpath -m "$2"); shift ;;
        --keep-appdir) keep=1 ;;
        -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

# Pinned tools (official releases; .work/ is not tracked).
tools=$repo/.work/tools/appimage
TOOL_URL=https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage
TOOL_SHA=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
RT_URL=https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64
RT_SHA=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
fetch() {   # url sha file
    if [ ! -f "$3" ] || ! echo "$2  $3" | sha256sum -c --quiet - 2>/dev/null; then
        echo "fetching $1" >&2
        mkdir -p "$(dirname "$3")"
        curl -fL --retry 3 -o "$3.part" "$1"
        echo "$2  $3.part" | sha256sum -c --quiet - || { echo "checksum mismatch: $1" >&2; exit 1; }
        mv "$3.part" "$3"
    fi
}
runtime=$tools/runtime-20251108-x86_64
fetch "$RT_URL" "$RT_SHA" "$runtime"
if [ -z "${APPIMAGETOOL:-}" ]; then
    APPIMAGETOOL=$tools/appimagetool-1.9.1-x86_64.AppImage
    fetch "$TOOL_URL" "$TOOL_SHA" "$APPIMAGETOOL"
    chmod +x "$APPIMAGETOOL"
fi

if [ "$do_build" = 1 ]; then
    (cd "$repo" && flock /tmp/xsg-build.lock cmake --build "$build" -j"$(nproc)")
fi

game=$build/xsg gpu=$build/gpu/xsg-gpu
for f in "$game" "$gpu"; do [ -x "$f" ] || { echo "missing $f (build the port first)" >&2; exit 1; }; done
file -L "$game" | grep -q 'ELF 32-bit.*Intel 80386\|ELF 32-bit.*i386' || { echo "$game is not an i386 ELF" >&2; exit 1; }
file -L "$gpu" | grep -q 'ELF 64-bit.*x86-64' || { echo "$gpu is not an x86-64 ELF" >&2; exit 1; }

# Host library locations (Arch: /usr/lib32 + /usr/lib; Debian: multiarch dirs).
lib32=$(dirname "$(readlink -f /lib/ld-linux.so.2)")
lib64=$(dirname "$(readlink -f /lib64/ld-linux-x86-64.so.2)")
[ -f "$lib32/libc.so.6" ] || { echo "no 32-bit glibc next to /lib/ld-linux.so.2 (install lib32-glibc to package)" >&2; exit 1; }
find64() {   # soname -> path, from the 64-bit ld cache
    local p
    p=$(ldconfig -p | sed -n "s/^[[:space:]]*$1 (libc6,x86-64[^)]*) => //p" | head -n1)
    [ -n "$p" ] || p=$lib64/$1
    [ -f "$p" ] || { echo "cannot find 64-bit $1" >&2; exit 1; }
    echo "$p"
}
find32() {
    local p=$lib32/$1
    [ -f "$p" ] || p=$(ldconfig -p | sed -n "s/^[[:space:]]*$1 (libc6) => //p" | head -n1)
    [ -n "$p" ] && [ -f "$p" ] || { echo "cannot find 32-bit $1" >&2; exit 1; }
    echo "$p"
}

work=$repo/.work/appimage
appdir=$work/AppDir
rm -rf "$appdir"
mkdir -p "$appdir"/usr/{bin/gpu,lib32,lib64/glibc,lib64/cxx,libexec/xsg,share/xsg,share/applications}
mkdir -p "$appdir"/usr/share/icons/hicolor/{scalable,256x256}/apps

# The glibc set: libc and the libraries split out of it before 2.34, so a host
# library that NEEDs libpthread.so.0 etc. cannot pull in a mismatched copy.
GLIBC_LIBS="libc.so.6 libm.so.6 libmvec.so.1 libpthread.so.0 libdl.so.2 librt.so.1 libresolv.so.2 libutil.so.1 libanl.so.1"

put() { install -m "${3:-755}" "$(readlink -f "$1")" "$2"; }

# ---- 32-bit game: usr/bin/xsg, run by usr/bin/ld-linux.so.2 (AppRun) ----
put "$game" "$appdir/usr/bin/xsg"
put "$lib32/ld-linux.so.2" "$appdir/usr/bin/ld-linux.so.2"
for l in $GLIBC_LIBS libstdc++.so.6 libgcc_s.so.1; do
    case $l in libmvec.so.1) [ -f "$lib32/$l" ] || continue ;; esac
    put "$(find32 "$l")" "$appdir/usr/lib32/$l"
done

# ---- 64-bit renderer: usr/libexec/xsg/xsg-gpu, run by usr/bin/gpu/xsg-gpu ----
put "$gpu" "$appdir/usr/libexec/xsg/xsg-gpu"
put "$here/xsg-gpu.sh" "$appdir/usr/bin/gpu/xsg-gpu"
put "$lib64/ld-linux-x86-64.so.2" "$appdir/usr/lib64/glibc/ld-linux-x86-64.so.2"
for l in $GLIBC_LIBS; do put "$(find64 "$l")" "$appdir/usr/lib64/glibc/$l"; done
for l in libstdc++.so.6 libgcc_s.so.1; do put "$(find64 "$l")" "$appdir/usr/lib64/cxx/$l"; done
put "$(find64 libSDL3.so.0)" "$appdir/usr/lib64/libSDL3.so.0"
getconf GNU_LIBC_VERSION | sed 's/^glibc //' > "$appdir/usr/lib64/glibc/VERSION"
grep -aoE 'GLIBCXX_3\.4\.[0-9]+' "$appdir/usr/lib64/cxx/libstdc++.so.6" | sort -Vu | tail -n1 > "$appdir/usr/lib64/cxx/VERSION"
# The 32-bit side always uses its bundled glibc; record its version too.
"$appdir/usr/bin/ld-linux.so.2" --version 2>/dev/null | head -n1 > "$appdir/usr/lib32/VERSION" || true

# xsg-gpu's own dependencies must all be bundled or be host-only on purpose.
missing=$(readelf -d "$gpu" "$(find64 libSDL3.so.0)" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | sort -u |
    while read -r so; do
        case " $GLIBC_LIBS libstdc++.so.6 libgcc_s.so.1 libSDL3.so.0 ld-linux-x86-64.so.2 " in *" $so "*) ;; *) echo "$so" ;; esac
    done)
[ -z "$missing" ] || { echo "xsg-gpu needs unbundled libraries: $missing" >&2; exit 1; }
missing=$(readelf -d "$game" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | while read -r so; do
    [ -f "$appdir/usr/lib32/$so" ] || echo "$so"; done)
[ -z "$missing" ] || { echo "xsg needs unbundled libraries: $missing" >&2; exit 1; }

# Runtime data: input prompt icons (CC0, port/assets/prompts).  xsg-gpu looks
# for them at a path compiled in from the source tree; see PACKAGING.md.
if [ -d "$repo/port/assets/prompts" ]; then
    cp -r "$repo/port/assets/prompts" "$appdir/usr/share/xsg/prompts"
fi

# The uncensored 3039C scene ([game] uncensored, MENU.md): <game dir>/uncensored/3039C_un.pss,
# when you have it, goes to uncensored/ at the AppImage root, where the game looks for it
# (disc.c xsg_cd_stream_override: two up from the loader in usr/bin).
gamedir=${XSG_GAME_DIR:-$repo/game}
mkdir -p "$appdir/uncensored"
if [ -f "$gamedir/uncensored/3039C_un.pss" ]; then
    put "$gamedir/uncensored/3039C_un.pss" "$appdir/uncensored/3039C_un.pss" 644
    echo "packing the uncensored 3039C scene ($(du -h "$appdir/uncensored/3039C_un.pss" | cut -f1))" >&2
else
    echo "no $gamedir/uncensored/3039C_un.pss: the uncensored folder is packed empty" >&2
fi
if [ -f "$gamedir/uncensored/3039C_un.cap" ]; then   # its English subtitles ([game] uncensored_subs)
    put "$gamedir/uncensored/3039C_un.cap" "$appdir/uncensored/3039C_un.cap" 644
fi

# ---- AppImage metadata ----
put "$here/AppRun" "$appdir/AppRun"
put "$here/xsg.desktop" "$appdir/usr/share/applications/xsg.desktop" 644
ln -s usr/share/applications/xsg.desktop "$appdir/xsg.desktop"
put "$here/xsg.svg" "$appdir/usr/share/icons/hicolor/scalable/apps/xsg.svg" 644
if command -v rsvg-convert >/dev/null; then
    rsvg-convert -w 256 -h 256 "$here/xsg.svg" -o "$appdir/usr/share/icons/hicolor/256x256/apps/xsg.png"
    ln -s usr/share/icons/hicolor/256x256/apps/xsg.png "$appdir/xsg.png"
    ln -s xsg.png "$appdir/.DirIcon"
else
    ln -s usr/share/icons/hicolor/scalable/apps/xsg.svg "$appdir/xsg.svg"
    ln -s xsg.svg "$appdir/.DirIcon"
fi

# ---- strip debug info from the bundled copies (port/build keeps -g) ----
before=$(du -sb "$appdir" | cut -f1)
find "$appdir/usr" -type f \( -name '*.so*' -o -name xsg -o -name xsg-gpu -o -name 'ld-linux*' \) -print0 |
    while IFS= read -r -d '' f; do
        file -b "$f" | grep -q '^ELF' || continue
        chmod u+w "$f"
        strip --strip-debug "$f"
    done
after=$(du -sb "$appdir" | cut -f1)

# ---- no game data may be in here ----
# (the one exception: uncensored/3039C_un.pss, packed above on purpose)
bad=$(find "$appdir" -path "$appdir/uncensored/3039C_un.pss" -prune -o \( -type f \( -iname '*.iso' -o -iname '*.bin' \
        -o -iname '*.img' -o -iname '*.7z' -o -iname 'SLUS_*' -o -iname '*.irx' -o -iname '*.pss' -o -iname '*.elf' \) \
        -o -size +60M -type f \) -print)
[ -z "$bad" ] || { echo "refusing to pack game files / unexpected large files:" >&2; echo "$bad" >&2; exit 1; }

echo >&2
echo "AppDir: $(numfmt --to=iec "$before") with debug info, $(numfmt --to=iec "$after") stripped" >&2
du -sh "$appdir"/usr/bin/xsg "$appdir"/usr/libexec/xsg/xsg-gpu "$appdir"/usr/lib32 "$appdir"/usr/lib64 >&2

rm -f "$out"
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL" --no-appstream --runtime-file "$runtime" \
    "$appdir" "$out" >&2
chmod +x "$out"
[ "$keep" = 1 ] || rm -rf "$appdir"
echo >&2
echo "$out: $(du -h "$out" | cut -f1)" >&2

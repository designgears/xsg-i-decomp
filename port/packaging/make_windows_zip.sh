#!/usr/bin/env bash
# Package the Windows build of the PC port as one zip: Xenosaga_Episode_I-Windows.zip.
# See port/docs/PACKAGING.md and port/docs/WINDOWS.md.
#
#   port/packaging/make_windows_zip.sh [--build] [--out FILE] [--keep-dir]
#
#   --build     configure + build the packaged tree first (XSG_PACKAGED=ON: no source-tree
#               paths compiled in), under the build lock
#   --out FILE  output path (default: <repo>/Xenosaga_Episode_I-Windows.zip)
#   --keep-dir  leave the staging folder in .work/winzip/Xenosaga_Episode_I afterwards
#
# Env: XSG_WIN_BUILD (default port/build-win32-pkg), XSG_GAME_DIR (build time only: where
# the build reads the IOP modules; default <repo>/game), XSG_PGS_SOURCE_DIR (optional).
set -euo pipefail

cat >&2 <<'EOF'
==============================================================================
 Xenosaga Episode I PC port -- Windows zip packager

 The binaries packed here (xsg.exe/xsg.dll, xsg-gpu.exe) hold the decompiled
 code and the layout of the game's data, but none of the game's bytes: the game
 reads its data from your own ISO at start-up ([game] iso in settings.ini, or
 the first-run setup screen).  If you pack uncensored/3039C_un.pss, that file
 is game data: keep such a package to yourself.
==============================================================================
EOF

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd "$here/../.." && pwd)
build=${XSG_WIN_BUILD:-$repo/port/build-win32-pkg}
out=$repo/Xenosaga_Episode_I-Windows.zip
keep=0 do_build=0
while [ $# -gt 0 ]; do
    case $1 in
        --build) do_build=1 ;;
        --out) out=$(realpath -m "$2"); shift ;;
        --keep-dir) keep=1 ;;
        -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

P32=i686-w64-mingw32 P64=x86_64-w64-mingw32
for t in $P32-gcc $P64-gcc $P32-objdump $P64-objdump $P32-strip $P64-strip zip; do
    command -v "$t" >/dev/null || { echo "missing tool: $t" >&2; exit 1; }
done

if [ "$do_build" = 1 ]; then
    args=(-G Ninja -DCMAKE_TOOLCHAIN_FILE="$repo/port/cmake/mingw-i686.cmake" -DXSG_PACKAGED=ON
          -DXSG_GAME_DIR="${XSG_GAME_DIR:-$repo/game}")
    [ -n "${XSG_PGS_SOURCE_DIR:-}" ] && args+=(-DXSG_PGS_SOURCE_DIR="$XSG_PGS_SOURCE_DIR")
    (cd "$repo" && cmake -S port -B "$build" "${args[@]}" >&2 &&
        flock /tmp/xsg-build.lock cmake --build "$build" -j"$(nproc)" >&2)
fi

exe=$build/xsg.exe dll=$build/xsg.dll gpu=$build/gpu/xsg-gpu.exe sdl=$build/gpu/SDL3.dll
for f in "$exe" "$dll" "$gpu" "$sdl"; do [ -f "$f" ] || { echo "missing $f (build first, or pass --build)" >&2; exit 1; }; done
file -L "$exe" "$dll" | grep -q 'PE32 .*Intel 80386\|PE32 .*Intel i386' || { echo "$exe/$dll are not 32-bit PE" >&2; exit 1; }
file -L "$gpu" | grep -q 'PE32+ .*x86-64' || { echo "$gpu is not a 64-bit PE" >&2; exit 1; }
grep -q 'XSG_PACKAGED:BOOL=ON' "$build/CMakeCache.txt" 2>/dev/null ||
    echo "warning: $build was not configured with -DXSG_PACKAGED=ON (source-tree paths are compiled in)" >&2

work=$repo/.work/winzip
stage=$work/Xenosaga_Episode_I
rm -rf "$stage"
mkdir -p "$stage/gpu" "$stage/share/xsg"

put() { install -m 644 "$1" "$2"; }
put "$exe" "$stage/xsg.exe"
put "$dll" "$stage/xsg.dll"
put "$gpu" "$stage/gpu/xsg-gpu.exe"
put "$sdl" "$stage/gpu/SDL3.dll"

# ---- MinGW runtime DLLs each binary imports (the default build links them statically,
#      so normally none; a build that links them dynamically gets the right 32/64-bit copies) ----
# A DLL that MinGW ships (libstdc++-6, libgcc_s_dw2-1 / libgcc_s_seh-1, libwinpthread-1, ...) is
# bundled from that toolchain; any other import is a Windows system DLL (or one we ship).
imports() { "$1-objdump" -p "$2" | sed -n 's/^[[:space:]]*DLL Name: //p'; }
mingw_dll() {   # prefix name -> path of the toolchain's copy, or nothing
    local f
    for f in "/usr/$1/bin/$2" "$("$1-gcc" -print-file-name="$2")"; do
        [ -f "$f" ] && { echo "$f"; return; }
    done
    find "/usr/$1" "/usr/lib/gcc/$1" -iname "$2" 2>/dev/null | head -n1
}
bundle_deps() {   # prefix binary destdir
    local p=$1 bin=$2 dest=$3 dll src
    for dll in $(imports "$p" "$bin"); do
        case $dll in api-ms-win-*|xsg.dll|SDL3.dll) continue ;; esac
        [ -f "$dest/$dll" ] && continue
        src=$(mingw_dll "$p" "$dll")
        if [ -z "$src" ]; then
            case $dll in lib*.dll) echo "$bin needs $dll, not found for $p" >&2; exit 1 ;; esac
            continue   # a system DLL
        fi
        echo "bundling $dll ($p) for $(basename "$bin")" >&2
        put "$src" "$dest/$dll"
        bundle_deps "$p" "$dest/$dll" "$dest"
    done
}
bundle_deps $P32 "$stage/xsg.dll" "$stage"
bundle_deps $P32 "$stage/xsg.exe" "$stage"
bundle_deps $P64 "$stage/gpu/xsg-gpu.exe" "$stage/gpu"
bundle_deps $P64 "$stage/gpu/SDL3.dll" "$stage/gpu"

# ---- runtime data: input prompt icons (CC0), found next to xsg-gpu.exe (ui/prompts.cpp) ----
if [ -d "$repo/port/assets/prompts" ]; then
    cp -r "$repo/port/assets/prompts" "$stage/share/xsg/prompts"
fi

# ---- the uncensored 3039C scene ([game] uncensored, MENU.md): uncensored\ next to xsg.exe,
# with <game dir>/uncensored/3039C_un.pss in it when you have it ----
gamedir=${XSG_GAME_DIR:-$repo/game}
mkdir -p "$stage/uncensored"
if [ -f "$gamedir/uncensored/3039C_un.pss" ]; then
    put "$gamedir/uncensored/3039C_un.pss" "$stage/uncensored/3039C_un.pss"
    echo "packing the uncensored 3039C scene ($(du -h "$stage/uncensored/3039C_un.pss" | cut -f1))" >&2
else
    echo "no $gamedir/uncensored/3039C_un.pss: the uncensored folder is packed empty" >&2
fi
if [ -f "$gamedir/uncensored/3039C_un.cap" ]; then   # its English subtitles ([game] uncensored_subs)
    put "$gamedir/uncensored/3039C_un.cap" "$stage/uncensored/3039C_un.cap"
fi

# ---- strip (the build tree keeps -g); keep the symbol tables for crash addresses ----
before=$(du -sb "$stage" | cut -f1)
for f in "$stage"/*.exe "$stage"/*.dll; do "$P32-strip" --strip-debug "$f"; done
for f in "$stage"/gpu/*.exe "$stage"/gpu/*.dll; do "$P64-strip" --strip-debug "$f"; done
after=$(du -sb "$stage" | cut -f1)

cat > "$stage/README.txt" <<'EOF'
Xenosaga Episode I -- PC port (Windows build)
=============================================

You need your own disc image of Xenosaga Episode I (USA, SLUS-20469).  This
package holds no game files: the game reads its data from your disc image when
it starts.

Running
  Double-click xsg.exe.  The first time, a setup screen asks for your disc
  image (the .iso of Xenosaga Episode I, USA).  It is remembered in
  settings.ini.  Keep the folder together: xsg.exe, xsg.dll, gpu\ and share\.

Requirements
  64-bit Windows 10 or 11 and a GPU with a Vulkan 1.1+ driver.

Files
  Settings      %APPDATA%\xsg\settings.ini   (F1 / Esc in game opens the menu)
  Memory card   %LOCALAPPDATA%\xsg\mc0
  Shader cache  %LOCALAPPDATA%\xsg\cache
  Screenshots   %USERPROFILE%\Pictures\xsg
  The XSG_CONFIG environment variable points the game at another settings file.

Movies need ffmpeg.exe on PATH (or [game] ffmpeg in settings.ini); without it
they are skipped.

The game prints its log to the console window; [debug] log_file in
settings.ini writes it to a file instead.
EOF
if [ -f "$stage/uncensored/3039C_un.pss" ]; then
    cat >> "$stage/README.txt" <<'EOF'

The uncensored 3039C scene is included (uncensored folder: the Japanese
release's uncut picture with the English voices, and its English subtitles).
Turn it on in Settings > Game > Uncensored 3039C; "English subtitles (3039C)"
shows the English lines at the top of the screen.
EOF
else
    cat >> "$stage/README.txt" <<'EOF'

The uncensored 3039C scene: put 3039C_un.pss (and its English subtitles,
3039C_un.cap) in the uncensored folder next to xsg.exe, then turn on
Settings > Game > Uncensored 3039C ([game] uncensored).
EOF
fi
sed -i 's/$/\r/' "$stage/README.txt"   # CRLF for Notepad

# ---- no game data may be in here ----
# (the one exception: uncensored/3039C_un.pss, packed above on purpose)
bad=$(find "$stage" -path "$stage/uncensored/3039C_un.pss" -prune -o \( -type f \( -iname '*.iso' -o -iname '*.bin' \
        -o -iname '*.img' -o -iname '*.7z' -o -iname 'SLUS_*' -o -iname '*.irx' -o -iname '*.pss' -o -iname '*.elf' \
        -o -iname '*.ovl' \) -o -size +60M -type f \) -print)
[ -z "$bad" ] || { echo "refusing to pack game files / unexpected large files:" >&2; echo "$bad" >&2; exit 1; }

echo >&2
echo "staging: $(numfmt --to=iec "$before") with debug info, $(numfmt --to=iec "$after") stripped" >&2
rm -f "$out"
(cd "$work" && zip -q -9 -r "$out" Xenosaga_Episode_I)
[ "$keep" = 1 ] || rm -rf "$stage"
echo "$out: $(du -h "$out" | cut -f1)" >&2

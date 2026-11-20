# Xenosaga Episode I -- host (Linux, i386) build skeleton.
#
#   cmake -S port -B port/build && cmake --build port/build -j
#
# Compiles every game C TU (src/main, src/ov01/02/10/11/12) straight from the
# shared sources with -DPORT_HOST.  Inline asm goes through
# include/port/ps2_asm.h, which stubs it on the host (see
# port/docs/STAGE2_PROGRESS.md).  Output: one object library per unit plus a
# static archive libxsg_game.a.  Nothing is linked into an executable yet.
cmake_minimum_required(VERSION 3.20)

# Stage 10: optimised by default (-O2). Files that give one symbol two C names
# go through XSG_VIEW (see scripts/find_alias_views.py).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Debug (-O0) or Release (-O2, default)" FORCE)
endif()
set(XSG_OPT "-O2" CACHE STRING "Release optimisation level (-O2; -O3 also works, see STAGE10_PERF.md)")
set(CMAKE_C_FLAGS_RELEASE "${XSG_OPT} -g" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${XSG_OPT} -g" CACHE STRING "" FORCE)
project(xsg_port C CXX)

get_filename_component(XSG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

option(PORT_ASM_TRAP "Stubbed inline asm traps (__builtin_trap) instead of being a no-op" OFF)
option(PORT_WERROR_C "Keep the C-level downgraded warnings as warnings (OFF = silence them)" ON)

set(XSG_UNITS main ov01 ov02 ov10 ov11 ov12)

set(XSG_SSP "-fno-stack-protector" CACHE STRING "Game C stack protector (-fstack-protector-all finds frame overruns)")
set(XSG_FLAGS
  -m32 -std=gnu89 -fno-strict-aliasing -fwrapv -ffp-contract=off -fsigned-char
  -fno-common -fgnu89-inline -fno-builtin -fno-pic ${XSG_SSP}
  -fno-delete-null-pointer-checks
  # Stage 10: data tables name file-static functions (link_xsg.py globalizes
  # them); at -O1+ unreferenced statics would be dropped or inlined away.
  -fkeep-static-functions -fkeep-static-consts
  # Stage 6: no SSE in game code. Quadword-aligned struct copies became
  # movdqa/movaps, which fault where a caller's Vector4 (or the i386 stack)
  # is not 16-byte aligned (Java_xeno_Light_setColor -> xglLightIntensityAmbient).
  -mno-sse -mno-sse2
  # ee-gcc 2.96 accepted these; modern gcc makes them errors.
  -Wno-error=implicit-function-declaration -Wno-error=implicit-int
  -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
  -Wno-error=return-mismatch -Wno-error=declaration-missing-parameter-type
  -include ${CMAKE_CURRENT_SOURCE_DIR}/include/ps2_host_compat.h
)
if(PORT_ASM_TRAP)
  list(APPEND XSG_FLAGS -DPORT_ASM_TRAP)
endif()
# Windows (MinGW): the Linux i386 ABI's struct layout and struct returns, which the
# game's data and its mismatched cross-TU prototypes were made to work with
# (MinGW defaults to MS bitfield layout and returns small structs in registers).
if(WIN32)
  # ...and 4-byte alignment of long long/double in structs (Linux i386; MinGW uses 8, which
  # moved UnduQuery's 64-bit masks past InitUwamono's 0x40-byte local copy of it).
  list(APPEND XSG_FLAGS -mno-ms-bitfields -fpcc-struct-return -mno-align-double)
endif()
# Stage 11: -DXSG_SANITIZE=undefined,float-cast-overflow instruments the game C
# (and the runtime's C) with UBSan; see STAGE11_UB.md.  The link adds the
# runtime library, otherwise link_xsg.py would autostub the __ubsan_* handlers.
set(XSG_SANITIZE "" CACHE STRING "Game C sanitizers (e.g. undefined,float-cast-overflow)")
set(XSG_SANITIZE_LIBS)
if(XSG_SANITIZE)
  list(APPEND XSG_FLAGS -fsanitize=${XSG_SANITIZE})
  set(XSG_SANITIZE_LIBS --libs=-fsanitize=${XSG_SANITIZE})
endif()
if(NOT PORT_WERROR_C)
  list(APPEND XSG_FLAGS -w)
endif()

set(XSG_OBJECTS)
# Every game file is optimised.  A file that gives one symbol two C names
# (`extern T FS_view __asm__("FS")` next to `FS`) must route the second name
# through XSG_VIEW() on the PORT_HOST side, or GCC caches loads across stores
# (STAGE10_PERF.md cause 4).  The checker fails the configure if one is left.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_alias_views.py
                        ${XSG_ROOT}/src
                RESULT_VARIABLE XSG_ALIAS_RC OUTPUT_VARIABLE XSG_ALIAS_OUT)
if(NOT XSG_ALIAS_RC EQUAL 0)
  message(FATAL_ERROR "unguarded alias views (wrap them in XSG_VIEW):\n${XSG_ALIAS_OUT}")
endif()
option(XSG_VU0_RUNS "Compile runs of VU0 macro ops to SSE (scripts/vu0_runs.py)" ON)
set(XSG_VU0_RUN_SRCS "")
foreach(unit IN LISTS XSG_UNITS)
  file(GLOB_RECURSE srcs CONFIGURE_DEPENDS "${XSG_ROOT}/src/${unit}/*.c")
  # src/main/nml_packet_add/ is an unreferenced work copy of a function that
  # nml_packet_add.c already defines; the PS2 build does not compile it.
  list(FILTER srcs EXCLUDE REGEX "/src/main/nml_packet_add/")
  list(SORT srcs)
  # VU0 op runs compiled to SSE (scripts/vu0_runs.py): files with runs of VU0
  # macro ops are built from a copy with each run replaced by one call into
  # the generated vu0_runs.c; the source tree is not touched.
  if(XSG_VU0_RUNS)
    execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/vu0_runs.py scan ${srcs}
                    OUTPUT_VARIABLE run_srcs OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    string(REPLACE "\n" ";" run_srcs "${run_srcs}")
    foreach(f IN LISTS run_srcs)
      file(RELATIVE_PATH rel "${XSG_ROOT}" "${f}")
      set(gen "${CMAKE_CURRENT_BINARY_DIR}/vu0gen/${rel}")
      get_filename_component(gdir "${gen}" DIRECTORY)
      get_filename_component(fdir "${f}" DIRECTORY)
      file(MAKE_DIRECTORY "${gdir}")
      add_custom_command(OUTPUT "${gen}"
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/vu0_runs.py rewrite "${f}" "${gen}"
        DEPENDS "${f}" ${CMAKE_CURRENT_SOURCE_DIR}/scripts/vu0_runs.py VERBATIM)
      set_source_files_properties("${gen}" PROPERTIES COMPILE_OPTIONS "-iquote;${fdir}")
      list(TRANSFORM srcs REPLACE "^${f}$" "${gen}")
      # link_xsg.py links every object in the unit's directory: drop the original's
      file(REMOVE "${CMAKE_BINARY_DIR}/CMakeFiles/xsg_${unit}.dir${f}.o" "${CMAKE_BINARY_DIR}/CMakeFiles/xsg_${unit}.dir${f}.obj")
      list(APPEND XSG_VU0_RUN_SRCS "${f}")
    endforeach()
  else()
    file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/CMakeFiles/xsg_${unit}.dir/vu0gen")
  endif()
  # Bisection aid: -DXSG_O0_REGEX=<regex> builds matching files at -O0.
  foreach(f IN LISTS srcs)
    if(XSG_O0_REGEX AND f MATCHES "${XSG_O0_REGEX}")
      set_source_files_properties(${f} PROPERTIES COMPILE_OPTIONS "-O0")
    endif()
  endforeach()
  add_library(xsg_${unit} OBJECT ${srcs})
  target_compile_definitions(xsg_${unit} PRIVATE PORT_HOST)
  target_compile_options(xsg_${unit} PRIVATE ${XSG_FLAGS})
  # Same include contract as the PS2 build: `-Iinclude -I.` from the repo root.
  target_include_directories(xsg_${unit} PRIVATE "${XSG_ROOT}/include" "${XSG_ROOT}")
  list(APPEND XSG_OBJECTS $<TARGET_OBJECTS:xsg_${unit}>)
endforeach()

add_library(xsg_game STATIC ${XSG_OBJECTS})
set_target_properties(xsg_game PROPERTIES LINKER_LANGUAGE C)

# ---------------------------------------------------------------------------
# Stage 3: host runtime + the linked executable port/build/xsg.
# Game data: port/data holds its layout only (labels, sizes, pointer words as
# relocations, and fill tables); the bytes are copied from the player's disc at
# start-up (runtime/datafill.c).  The build needs no game files.  Regenerate
# port/data after a decomp change with port/scripts/gen_data.sh (needs the PS2
# build in build/<unit>, made by configure.py from the disc).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(XSG_GAME_DIR "${XSG_ROOT}/game" CACHE PATH "Default game directory (iso/, extracted-iso/)")

set(XSG_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
file(GLOB XSG_RUNTIME_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.c")
list(APPEND XSG_RUNTIME_SRCS ${XSG_DATA_DIR}/fill_index.c)
file(GLOB_RECURSE XSG_DATA_SKELETON CONFIGURE_DEPENDS "${XSG_DATA_DIR}/*.s" "${XSG_DATA_DIR}/syms/*")
# Windows (port/docs/WINDOWS.md): MinGW-w64 i686 cross build, e.g.
#   cmake -S port -B port/build-win32 -DCMAKE_TOOLCHAIN_FILE=$PWD/port/cmake/mingw-i686.cmake
# The runtime gets a platform layer (runtime/win32/) and a force-included POSIX
# compat header; the game is linked into xsg.dll, loaded by the stub xsg.exe.
set(XSG_W32_DIR "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32")
if(WIN32)
  list(APPEND XSG_RUNTIME_SRCS ${XSG_W32_DIR}/platform_win32.c)
  set(XSG_W32_RT_FLAGS -include ${XSG_W32_DIR}/xsg_win_compat.h)
  set(XSG_W32_RT_INCS ${XSG_W32_DIR}/include ${XSG_W32_DIR})
endif()
# Stage 7: the resident VU0 microprogram, assembled from src/main/vu0/Vu0MicroCode.dvp
# by the PS2 build; port/data/vu0_microcode.c is gen_vu0_microcode.py's output (gen_data.sh).
# XSG_GPU_NATIVE fast path (gs_bridge.cpp): set-0 strip batches run the VU1 C
# reference in place instead of the interpreter.  Same float rules as vu1.cpp.
file(GLOB XSG_VU1REF_FAST CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/render/vu1_ref/*.c")
set(XSG_VU0_RUNS_C "")
if(XSG_VU0_RUNS)
  set(XSG_VU0_RUNS_C "${CMAKE_CURRENT_BINARY_DIR}/vu0gen/vu0_runs.c")
  add_custom_command(OUTPUT "${XSG_VU0_RUNS_C}"
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/vu0_runs.py runs "${XSG_VU0_RUNS_C}" ${XSG_VU0_RUN_SRCS}
    DEPENDS ${XSG_VU0_RUN_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/vu0_runs.py VERBATIM)
  set_source_files_properties("${XSG_VU0_RUNS_C}" PROPERTIES COMPILE_OPTIONS "-O2;-msse2;-mfpmath=sse")
endif()
add_library(xsg_runtime OBJECT ${XSG_RUNTIME_SRCS} ${XSG_DATA_DIR}/vu0_microcode.c ${XSG_VU1REF_FAST} ${XSG_VU0_RUNS_C})
target_include_directories(xsg_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
set_source_files_properties(${XSG_VU1REF_FAST} PROPERTIES COMPILE_OPTIONS "-O3;-msse2;-mfpmath=sse;-ffp-contract=off")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu0.c" PROPERTIES COMPILE_OPTIONS "-O2;-msse2;-mfpmath=sse")
# Movie and IPU decoders: SSE float math with -fno-math-errno makes lrintf one cvtss2si.
# Otherwise every rounded pixel is a call into the C library, and Windows' ucrtbase
# lrintf is slow enough to hold the logo and the movies far below 60 fps (#3).
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/runtime/mpeg2dec.c" PROPERTIES COMPILE_OPTIONS "-O2;-msse2;-mfpmath=sse;-fno-math-errno")
target_compile_options(xsg_runtime PRIVATE -m32 -std=gnu11 -D_FILE_OFFSET_BITS=64 -O1 -g -Wall -Wno-unused-parameter
  -fno-pic -fno-stack-protector -fno-omit-frame-pointer)
# XSG_PACKAGED (port/packaging): no source/build-tree paths compiled in.  XSG_GAME_DIR is then
# only where the build reads the IOP modules; at run time the game uses settings.ini or the
# first-run setup screen.
option(XSG_PACKAGED "Build for a package: no source/build-tree paths compiled in" OFF)
if(XSG_PACKAGED)
  set(XSG_RUNTIME_GAME_DIR "")
  set(XSG_RUNTIME_SHOT_DIR "shots")
else()
  set(XSG_RUNTIME_GAME_DIR "${XSG_GAME_DIR}")
  set(XSG_RUNTIME_SHOT_DIR "${CMAKE_BINARY_DIR}/shots")
endif()
target_compile_definitions(xsg_runtime PRIVATE "XSG_DEFAULT_GAME_DIR=\"${XSG_RUNTIME_GAME_DIR}\"")
if(WIN32)
  target_compile_options(xsg_runtime PRIVATE ${XSG_W32_RT_FLAGS} -mno-ms-bitfields)
  target_include_directories(xsg_runtime BEFORE PRIVATE ${XSG_W32_RT_INCS} "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
endif()

# Stage 5: software GS (vendored, MIT, see third_party/ps2-port-engine-gs/) and its bridge.
set(XSG_GS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ps2-port-engine-gs")
file(GLOB XSG_GS_SRCS CONFIGURE_DEPENDS "${XSG_GS_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.cpp")
add_library(xsg_gs OBJECT ${XSG_GS_SRCS})
target_compile_options(xsg_gs PRIVATE -m32 -std=c++17 -O2 -g -msse4.1 -fno-pic -fno-stack-protector -w)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu1.cpp" PROPERTIES COMPILE_OPTIONS "-mfpmath=sse;-msse2")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/runtime/ipu.cpp" PROPERTIES COMPILE_OPTIONS "-mfpmath=sse;-msse2;-fno-math-errno")
target_include_directories(xsg_gs PRIVATE "${XSG_GS_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
target_compile_definitions(xsg_gs PRIVATE "XSG_DEFAULT_SHOT_DIR=\"${XSG_RUNTIME_SHOT_DIR}\"")
if(WIN32)
  # The 32-bit CRT's malloc aligns to 8 while GCC assumes 16 for operator new:
  # over-aligned types (vu1.cpp's 16-byte DP) must take the aligned new.
  target_compile_options(xsg_gs PRIVATE ${XSG_W32_RT_FLAGS} -mno-ms-bitfields $<$<COMPILE_LANGUAGE:CXX>:-faligned-new=8>)
  target_include_directories(xsg_gs BEFORE PRIVATE ${XSG_W32_RT_INCS})
endif()

# Stage 10: the IOP sound driver (src/audio, SSD.IRX + RSSD.IRX) built for the
# host and run by runtime/iop_sound.c.  Its tables' layout is
# port/data/iop_sound_data.S (gen_iop_sound_data.py --skeleton); their bytes come from
# the disc's IRX files at start-up (datafill.c).  The whole driver is one relocatable object
# with every symbol prefixed `ssdiop_`, so it cannot clash with the EE code.
file(GLOB XSG_SSD_SRCS CONFIGURE_DEPENDS "${XSG_ROOT}/src/audio/ssd/*.c")
file(GLOB XSG_RSSD_SRCS CONFIGURE_DEPENDS "${XSG_ROOT}/src/audio/rssd/*.c")
set(XSG_SOUND_FLAGS -m32 -std=gnu89 -O1 -g -w -fno-strict-aliasing -fwrapv -fno-common -fno-builtin
  -fno-pic -fno-stack-protector -mno-sse -mno-sse2 -Wno-error=incompatible-pointer-types
  -Wno-error=implicit-function-declaration -Wno-error=int-conversion -DPORT_HOST)
# The driver's symbols all get the ssdiop_ prefix.  ELF: objcopy --prefix-symbols=ssdiop_.
# COFF (Windows) prefixes C names with `_`: the driver is compiled without it and
# prefixed with `_ssdiop_`, which gives the C names ssdiop_* again.
if(WIN32)
  list(APPEND XSG_SOUND_FLAGS -fno-leading-underscore -mno-ms-bitfields -mno-align-double)
  set(XSG_LD_R ${XSG_MINGW_PREFIX}-ld -r)
  # (objcopy --prefix-symbols would rename COFF section symbols too)
  set(XSG_PREFIX_SSDIOP ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/coff_prefix_symbols.py
      ${XSG_MINGW_PREFIX}-nm ${XSG_MINGW_PREFIX}-objcopy _ssdiop_)
else()
  set(XSG_LD_R ld -m elf_i386 -r)
  set(XSG_PREFIX_SSDIOP objcopy --prefix-symbols=ssdiop_)
endif()
# COFF: no weak data definitions (scripts/coff_weak_defs.py); ELF uses the file as generated.
if(WIN32)
  set(XSG_IOP_DATA_SUFFIX _coff)
  set(XSG_IOP_DATA_FIX ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/coff_weak_defs.py
      ${XSG_MINGW_PREFIX}-nm ${XSG_DATA_DIR}/iop_sound_data.S ${CMAKE_BINARY_DIR}/iop_sound_data_coff.S
      $<TARGET_OBJECTS:xsg_ssd> $<TARGET_OBJECTS:xsg_rssd>)
else()
  set(XSG_IOP_DATA_SUFFIX "")
  set(XSG_IOP_DATA_FIX ${CMAKE_COMMAND} -E true)
endif()
add_library(xsg_ssd OBJECT ${XSG_SSD_SRCS})
target_compile_options(xsg_ssd PRIVATE ${XSG_SOUND_FLAGS} -D_fdata=ssd_fdata)
add_library(xsg_rssd OBJECT ${XSG_RSSD_SRCS})
target_compile_options(xsg_rssd PRIVATE ${XSG_SOUND_FLAGS} -D_fdata=rssd_fdata)
if(WIN32)
  set(XSG_IOP_DATA_S ${CMAKE_BINARY_DIR}/iop_sound_data_coff.S)
else()
  set(XSG_IOP_DATA_S ${XSG_DATA_DIR}/iop_sound_data.S)
endif()
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/iop_sound_driver.o
  COMMAND ${XSG_IOP_DATA_FIX}
  COMMAND ${CMAKE_C_COMPILER} -m32 -c ${XSG_IOP_DATA_S} -o ${CMAKE_BINARY_DIR}/iop_sound_data.o
  COMMAND ${XSG_LD_R} -o ${CMAKE_BINARY_DIR}/iop_sound_driver.r.o
          $<TARGET_OBJECTS:xsg_ssd> $<TARGET_OBJECTS:xsg_rssd> ${CMAKE_BINARY_DIR}/iop_sound_data.o
  COMMAND ${XSG_PREFIX_SSDIOP} ${CMAKE_BINARY_DIR}/iop_sound_driver.r.o ${CMAKE_BINARY_DIR}/iop_sound_driver.o
  DEPENDS xsg_ssd xsg_rssd $<TARGET_OBJECTS:xsg_ssd> $<TARGET_OBJECTS:xsg_rssd> ${XSG_DATA_DIR}/iop_sound_data.S
  COMMAND_EXPAND_LISTS
  COMMENT "Linking the host IOP sound driver")

# The overlap check reads the PS2 build's symbol tables and splat labels
# (build/<unit>); a checkout without the PS2 build (CI) skips it.
if(EXISTS "${XSG_ROOT}/build/main/symbol_addrs.txt")
  set(XSG_OVERLAP_CHECK ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_overlap_access.py
      ${CMAKE_BINARY_DIR} --root ${XSG_ROOT})
else()
  set(XSG_OVERLAP_CHECK ${CMAKE_COMMAND} -E echo "find_overlap_access.py skipped: no PS2 build in ${XSG_ROOT}/build")
endif()

# Stage 11: fail if a game file reaches one PS2 data symbol through another
# that GCC treats as a separate object (find_overlap_access.py; needs the
# compiled objects, so it runs here rather than at configure time).
if(WIN32)
  # Windows: xsg.dll (the game + runtime, linked like the Linux executable but as a
  # DLL at a fixed base), then the stub xsg.exe that reserves PS2 RAM and loads it
  # (runtime/win32/stub.c).  find_overlap_access.py already gates the same sources
  # in the Linux build.  winpthreads and the GCC runtime are static.
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/xsg.dll ${CMAKE_BINARY_DIR}/xsg.dll.a
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/link_xsg.py
            --target pe --tool-prefix ${XSG_MINGW_PREFIX}-
            --build ${CMAKE_BINARY_DIR} --data ${XSG_DATA_DIR} --out ${CMAKE_BINARY_DIR}/xsg.dll --cc ${CMAKE_C_COMPILER}
            --runtime $<TARGET_OBJECTS:xsg_runtime> $<TARGET_OBJECTS:xsg_gs> ${CMAKE_BINARY_DIR}/iop_sound_driver.o
            --libs=-static --libs=-static-libgcc --libs=-static-libstdc++ --libs=-lstdc++ --libs=-lwinpthread
            --libs=-lwinmm --libs=-lm
    DEPENDS xsg_game xsg_runtime xsg_gs $<TARGET_OBJECTS:xsg_runtime> $<TARGET_OBJECTS:xsg_gs>
            ${CMAKE_BINARY_DIR}/iop_sound_driver.o
            ${XSG_DATA_SKELETON}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/link_xsg.py
    COMMAND_EXPAND_LISTS
    COMMENT "Linking xsg.dll")
  # icon + version resource (port/packaging/windows)
  include(${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/resources.cmake)
  xsg_windows_rc(XSG_STUB_RC xsg "Xenosaga Episode I (PC port)")
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/xsg_res.o
    COMMAND ${XSG_MINGW_PREFIX}-windres ${XSG_STUB_RC} -O coff -o ${CMAKE_BINARY_DIR}/xsg_res.o
    DEPENDS ${XSG_STUB_RC} ${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/xsg.rc.in)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/xsg.exe
    COMMAND ${CMAKE_C_COMPILER} -O1 -nostdlib -fno-asynchronous-unwind-tables -fno-stack-protector
            ${XSG_W32_DIR}/stub.c ${CMAKE_BINARY_DIR}/xsg_res.o ${CMAKE_BINARY_DIR}/xsg.dll.a -o ${CMAKE_BINARY_DIR}/xsg.exe
            -Wl,-e,_stub_entry@0 -Wl,--image-base,0x10000 -Wl,--disable-dynamicbase
            -Wl,--disable-reloc-section -Wl,--large-address-aware -Wl,--subsystem,console
    DEPENDS ${XSG_W32_DIR}/stub.c ${CMAKE_BINARY_DIR}/xsg.dll.a ${CMAKE_BINARY_DIR}/xsg_res.o
    COMMENT "Linking the stub xsg.exe")
  add_custom_target(xsg_exe ALL DEPENDS ${CMAKE_BINARY_DIR}/xsg.dll ${CMAKE_BINARY_DIR}/xsg.exe)
else()
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/xsg
  COMMAND ${XSG_OVERLAP_CHECK}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/link_xsg.py
          --build ${CMAKE_BINARY_DIR} --data ${XSG_DATA_DIR} --out ${CMAKE_BINARY_DIR}/xsg --cc ${CMAKE_C_COMPILER}
          --runtime $<TARGET_OBJECTS:xsg_runtime> $<TARGET_OBJECTS:xsg_gs> ${CMAKE_BINARY_DIR}/iop_sound_driver.o --libs=-lm --libs=-lstdc++ --libs=-pthread ${XSG_SANITIZE_LIBS}
  DEPENDS xsg_game xsg_runtime xsg_gs $<TARGET_OBJECTS:xsg_runtime> $<TARGET_OBJECTS:xsg_gs>
          ${CMAKE_BINARY_DIR}/iop_sound_driver.o
          ${XSG_DATA_SKELETON}
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/link_xsg.py
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_overlap_access.py
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_overlap_access_allow.txt
  COMMAND_EXPAND_LISTS
  COMMENT "Linking host executable xsg")
add_custom_target(xsg_exe ALL DEPENDS ${CMAKE_BINARY_DIR}/xsg)
endif()

# Stage 9: xsg-gpu, the 64-bit paraLLEl-GS renderer process (port/gpu/, own
# CMake project because it is not -m32; paraLLEl-GS is fetched and linked only
# there).  Output: ${CMAKE_BINARY_DIR}/gpu/xsg-gpu, found by the game for XSG_GS=gpu|both.
option(XSG_GPU "Build the xsg-gpu hardware renderer (fetches paraLLEl-GS)" ON)
set(XSG_PGS_SOURCE_DIR "" CACHE PATH "Use this paraLLEl-GS checkout instead of fetching it")
if(XSG_GPU)
  include(ExternalProject)
  set(_xsg_gpu_args -DCMAKE_BUILD_TYPE=Release -DXSG_PACKAGED=${XSG_PACKAGED})
  if(WIN32)
    # xsg-gpu.exe is 64-bit: the other MinGW toolchain; it lands in <build>/gpu/ next to SDL3.dll.
    list(APPEND _xsg_gpu_args -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/mingw-x86_64.cmake)
    if(XSG_SDL3_DIR)
      list(APPEND _xsg_gpu_args -DXSG_SDL3_DIR=${XSG_SDL3_DIR})
    endif()
  endif()
  if(XSG_PGS_SOURCE_DIR)
    list(APPEND _xsg_gpu_args -DFETCHCONTENT_SOURCE_DIR_PARALLEL_GS=${XSG_PGS_SOURCE_DIR})
  endif()
  ExternalProject_Add(xsg_gpu
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gpu
    BINARY_DIR ${CMAKE_BINARY_DIR}/gpu
    CMAKE_ARGS ${_xsg_gpu_args}
    BUILD_ALWAYS ON
    INSTALL_COMMAND "")
endif()

# Stage 7: VU0 model unit tests (port/tests/test_vu0.c); run port/build*/test_vu0.
add_executable(test_vu0 EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vu0.c ${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu0.c
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu1.cpp ${CMAKE_CURRENT_SOURCE_DIR}/runtime/settings.c ${XSG_DATA_DIR}/vu0_microcode.c
  ${XSG_ROOT}/src/main/m_math.c)
set_source_files_properties(${XSG_ROOT}/src/main/m_math.c TARGET_DIRECTORY test_vu0 PROPERTIES
  COMPILE_OPTIONS "${XSG_FLAGS};-DPORT_HOST;-I${XSG_ROOT}/include;-I${XSG_ROOT}")
target_include_directories(test_vu0 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
target_compile_options(test_vu0 PRIVATE -m32 -O1 -g -fno-pic -msse2 -mfpmath=sse)
target_link_options(test_vu0 PRIVATE -m32 -no-pie)
target_link_libraries(test_vu0 m stdc++)

# VU1 C references + capture test (native renderer groundwork).
include(${CMAKE_CURRENT_SOURCE_DIR}/render/vu1_ref.cmake)

# Stage 10: VU1 interpreter replay against XSG_VU1_CAPTURE files (bit-identical check + bench).
add_executable(test_vu1_replay EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vu1_replay.cpp ${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu1.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/prof.c ${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu0.c ${CMAKE_CURRENT_SOURCE_DIR}/runtime/settings.c ${XSG_DATA_DIR}/vu0_microcode.c)
target_include_directories(test_vu1_replay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
target_compile_options(test_vu1_replay PRIVATE -m32 -O2 -g -fno-pic -msse4.1 -mfpmath=sse)
target_link_options(test_vu1_replay PRIVATE -m32 -no-pie)
target_link_libraries(test_vu1_replay m stdc++)
# Stage 10: software SPU2 checks (port/tests/test_spu2.c); run port/build*/test_spu2.
add_executable(test_spu2 EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_spu2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/spu2.c ${CMAKE_CURRENT_SOURCE_DIR}/runtime/spu2_tables.c
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/spu2_libsd_tables.c)
target_include_directories(test_spu2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
target_compile_options(test_spu2 PRIVATE -m32 -O1 -g -Wall)
target_link_options(test_spu2 PRIVATE -m32)
target_link_libraries(test_spu2 m)

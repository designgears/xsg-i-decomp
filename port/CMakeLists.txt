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
foreach(unit IN LISTS XSG_UNITS)
  file(GLOB_RECURSE srcs CONFIGURE_DEPENDS "${XSG_ROOT}/src/${unit}/*.c")
  # src/main/nml_packet_add/ is an unreferenced work copy of a function that
  # nml_packet_add.c already defines; the PS2 build does not compile it.
  list(FILTER srcs EXCLUDE REGEX "/src/main/nml_packet_add/")
  list(SORT srcs)
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
# Game data comes from the PS2 build's own outputs (build/<unit>/..., made by
# configure.py from the user's disc); gen_host_data.py converts it for i386.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(XSG_GAME_DIR "${XSG_ROOT}/game" CACHE PATH "Default game directory (iso/, extracted-iso/)")

file(GLOB XSG_RUNTIME_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.c")
# Stage 7: the resident VU0 microprogram, from the PS2 build's DVP object.
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/vu0_microcode.c
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_vu0_microcode.py
          ${XSG_ROOT}/build/main/build/vu0/Vu0MicroCode.o ${CMAKE_BINARY_DIR}/vu0_microcode.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_vu0_microcode.py ${XSG_ROOT}/build/main/build/vu0/Vu0MicroCode.o)
# XSG_GPU_NATIVE fast path (gs_bridge.cpp): set-0 strip batches run the VU1 C
# reference in place instead of the interpreter.  Same float rules as vu1.cpp.
file(GLOB XSG_VU1REF_FAST CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/render/vu1_ref/*.c")
add_library(xsg_runtime OBJECT ${XSG_RUNTIME_SRCS} ${CMAKE_BINARY_DIR}/vu0_microcode.c ${XSG_VU1REF_FAST})
set_source_files_properties(${XSG_VU1REF_FAST} PROPERTIES COMPILE_OPTIONS "-O2;-msse2;-mfpmath=sse;-ffp-contract=off")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu0.c" PROPERTIES COMPILE_OPTIONS "-msse2;-mfpmath=sse")
target_compile_options(xsg_runtime PRIVATE -m32 -std=gnu11 -D_FILE_OFFSET_BITS=64 -O1 -g -Wall -Wno-unused-parameter
  -fno-pic -fno-stack-protector -fno-omit-frame-pointer)
target_compile_definitions(xsg_runtime PRIVATE "XSG_DEFAULT_GAME_DIR=\"${XSG_GAME_DIR}\"")

# Stage 5: software GS (vendored, MIT, see third_party/ps2-port-engine-gs/) and its bridge.
set(XSG_GS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ps2-port-engine-gs")
file(GLOB XSG_GS_SRCS CONFIGURE_DEPENDS "${XSG_GS_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.cpp")
add_library(xsg_gs OBJECT ${XSG_GS_SRCS})
target_compile_options(xsg_gs PRIVATE -m32 -std=c++17 -O2 -g -msse4.1 -fno-pic -fno-stack-protector -w)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu1.cpp" PROPERTIES COMPILE_OPTIONS "-mfpmath=sse;-msse2")
target_include_directories(xsg_gs PRIVATE "${XSG_GS_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
target_compile_definitions(xsg_gs PRIVATE "XSG_DEFAULT_SHOT_DIR=\"${CMAKE_BINARY_DIR}/shots\"")

# Stage 10: the IOP sound driver (src/audio, SSD.IRX + RSSD.IRX) built for the
# host and run by runtime/iop_sound.c.  Its tables come from the user's IRX
# files (gen_iop_sound_data.py); the whole driver is one relocatable object
# with every symbol prefixed `ssdiop_`, so it cannot clash with the EE code.
set(XSG_IOP_DIR "${XSG_GAME_DIR}/extracted-iso/IOP")
file(GLOB XSG_SSD_SRCS CONFIGURE_DEPENDS "${XSG_ROOT}/src/audio/ssd/*.c")
file(GLOB XSG_RSSD_SRCS CONFIGURE_DEPENDS "${XSG_ROOT}/src/audio/rssd/*.c")
set(XSG_SOUND_FLAGS -m32 -std=gnu89 -O1 -g -w -fno-strict-aliasing -fwrapv -fno-common -fno-builtin
  -fno-pic -fno-stack-protector -mno-sse -mno-sse2 -Wno-error=incompatible-pointer-types
  -Wno-error=implicit-function-declaration -Wno-error=int-conversion -DPORT_HOST)
add_library(xsg_ssd OBJECT ${XSG_SSD_SRCS})
target_compile_options(xsg_ssd PRIVATE ${XSG_SOUND_FLAGS} -D_fdata=ssd_fdata)
add_library(xsg_rssd OBJECT ${XSG_RSSD_SRCS})
target_compile_options(xsg_rssd PRIVATE ${XSG_SOUND_FLAGS} -D_fdata=rssd_fdata)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/iop_sound_data.S
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_iop_sound_data.py
          ${XSG_IOP_DIR}/SSD.IRX ${XSG_IOP_DIR}/RSSD.IRX ${CMAKE_BINARY_DIR}/iop_sound_data.S
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_iop_sound_data.py
          ${XSG_IOP_DIR}/SSD.IRX ${XSG_IOP_DIR}/RSSD.IRX
  COMMENT "Extracting the IOP sound driver's data")
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/iop_sound_driver.o
  COMMAND ${CMAKE_C_COMPILER} -m32 -c ${CMAKE_BINARY_DIR}/iop_sound_data.S -o ${CMAKE_BINARY_DIR}/iop_sound_data.o
  COMMAND ld -m elf_i386 -r -o ${CMAKE_BINARY_DIR}/iop_sound_driver.r.o
          $<TARGET_OBJECTS:xsg_ssd> $<TARGET_OBJECTS:xsg_rssd> ${CMAKE_BINARY_DIR}/iop_sound_data.o
  COMMAND objcopy --prefix-symbols=ssdiop_ ${CMAKE_BINARY_DIR}/iop_sound_driver.r.o ${CMAKE_BINARY_DIR}/iop_sound_driver.o
  DEPENDS xsg_ssd xsg_rssd $<TARGET_OBJECTS:xsg_ssd> $<TARGET_OBJECTS:xsg_rssd> ${CMAKE_BINARY_DIR}/iop_sound_data.S
  COMMAND_EXPAND_LISTS
  COMMENT "Linking the host IOP sound driver")

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/data/data_sources.cmake
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_host_data.py
          --root ${XSG_ROOT} --out ${CMAKE_BINARY_DIR}/data
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_host_data.py
          ${XSG_ROOT}/build/main/main.elf.ld
  COMMENT "Converting PS2 game data for the host")

# Stage 11: fail if a game file reaches one PS2 data symbol through another
# that GCC treats as a separate object (find_overlap_access.py; needs the
# compiled objects, so it runs here rather than at configure time).
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/xsg
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_overlap_access.py
          ${CMAKE_BINARY_DIR} --root ${XSG_ROOT}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/link_xsg.py
          --build ${CMAKE_BINARY_DIR} --out ${CMAKE_BINARY_DIR}/xsg --cc ${CMAKE_C_COMPILER}
          --runtime $<TARGET_OBJECTS:xsg_runtime> $<TARGET_OBJECTS:xsg_gs> ${CMAKE_BINARY_DIR}/iop_sound_driver.o --libs=-lm --libs=-lstdc++ --libs=-pthread ${XSG_SANITIZE_LIBS}
  DEPENDS xsg_game xsg_runtime xsg_gs $<TARGET_OBJECTS:xsg_runtime> $<TARGET_OBJECTS:xsg_gs>
          ${CMAKE_BINARY_DIR}/iop_sound_driver.o
          ${CMAKE_BINARY_DIR}/data/data_sources.cmake
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/link_xsg.py
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_overlap_access.py
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_overlap_access_allow.txt
  COMMAND_EXPAND_LISTS
  COMMENT "Linking host executable xsg")
add_custom_target(xsg_exe ALL DEPENDS ${CMAKE_BINARY_DIR}/xsg)

# Stage 9: xsg-gpu, the 64-bit paraLLEl-GS renderer process (port/gpu/, own
# CMake project because it is not -m32; paraLLEl-GS is fetched and linked only
# there).  Output: ${CMAKE_BINARY_DIR}/gpu/xsg-gpu, found by the game for XSG_GS=gpu|both.
option(XSG_GPU "Build the xsg-gpu hardware renderer (fetches paraLLEl-GS)" ON)
set(XSG_PGS_SOURCE_DIR "" CACHE PATH "Use this paraLLEl-GS checkout instead of fetching it")
if(XSG_GPU)
  include(ExternalProject)
  set(_xsg_gpu_args -DCMAKE_BUILD_TYPE=Release)
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
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu1.cpp ${CMAKE_BINARY_DIR}/vu0_microcode.c
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
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime/prof.c ${CMAKE_CURRENT_SOURCE_DIR}/runtime/vu0.c ${CMAKE_BINARY_DIR}/vu0_microcode.c)
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

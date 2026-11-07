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
project(xsg_port C CXX)

get_filename_component(XSG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

option(PORT_ASM_TRAP "Stubbed inline asm traps (__builtin_trap) instead of being a no-op" OFF)
option(PORT_WERROR_C "Keep the C-level downgraded warnings as warnings (OFF = silence them)" ON)

set(XSG_UNITS main ov01 ov02 ov10 ov11 ov12)

set(XSG_FLAGS
  -m32 -std=gnu89 -fno-strict-aliasing -fwrapv -ffp-contract=off -fsigned-char
  -fno-common -fgnu89-inline -fno-builtin -fno-pic -fno-stack-protector
  -fno-delete-null-pointer-checks
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
if(NOT PORT_WERROR_C)
  list(APPEND XSG_FLAGS -w)
endif()

set(XSG_OBJECTS)
foreach(unit IN LISTS XSG_UNITS)
  file(GLOB_RECURSE srcs CONFIGURE_DEPENDS "${XSG_ROOT}/src/${unit}/*.c")
  # src/main/nml_packet_add/ is an unreferenced work copy of a function that
  # nml_packet_add.c already defines; the PS2 build does not compile it.
  list(FILTER srcs EXCLUDE REGEX "/src/main/nml_packet_add/")
  list(SORT srcs)
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
add_library(xsg_runtime OBJECT ${XSG_RUNTIME_SRCS})
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

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/data/data_sources.cmake
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_host_data.py
          --root ${XSG_ROOT} --out ${CMAKE_BINARY_DIR}/data
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_host_data.py
          ${XSG_ROOT}/build/main/main.elf.ld
  COMMENT "Converting PS2 game data for the host")

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/xsg
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/link_xsg.py
          --build ${CMAKE_BINARY_DIR} --out ${CMAKE_BINARY_DIR}/xsg --cc ${CMAKE_C_COMPILER}
          --runtime $<TARGET_OBJECTS:xsg_runtime> $<TARGET_OBJECTS:xsg_gs> --libs=-lm --libs=-lstdc++ --libs=-pthread
  DEPENDS xsg_game xsg_runtime xsg_gs $<TARGET_OBJECTS:xsg_runtime> $<TARGET_OBJECTS:xsg_gs>
          ${CMAKE_BINARY_DIR}/data/data_sources.cmake
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/link_xsg.py
  COMMAND_EXPAND_LISTS
  COMMENT "Linking host executable xsg")
add_custom_target(xsg_exe ALL DEPENDS ${CMAKE_BINARY_DIR}/xsg)

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
project(xsg_port C)

get_filename_component(XSG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

option(PORT_ASM_TRAP "Stubbed inline asm traps (__builtin_trap) instead of being a no-op" OFF)
option(PORT_WERROR_C "Keep the C-level downgraded warnings as warnings (OFF = silence them)" ON)

set(XSG_UNITS main ov01 ov02 ov10 ov11 ov12)

set(XSG_FLAGS
  -m32 -std=gnu89 -fno-strict-aliasing -fwrapv -ffp-contract=off -fsigned-char
  -fno-common -fgnu89-inline -fno-builtin -fno-pic -fno-stack-protector
  -fno-delete-null-pointer-checks
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

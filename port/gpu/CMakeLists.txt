# xsg-gpu: the 64-bit hardware GS renderer process (Stage 9, see port/docs/STAGE9_GPU.md).
#
# A separate CMake project because the game is built -m32 and this is a native
# 64-bit executable.  port/CMakeLists.txt builds it with ExternalProject; it can
# also be built on its own:
#
#   cmake -S port/gpu -B port/build/gpu && cmake --build port/build/gpu -j
#
# paraLLEl-GS (LGPLv3+, https://github.com/Arntzen-Software/parallel-gs) and its
# Granite submodule are fetched at configure time, pinned to one commit, and are
# linked ONLY into this executable.  Nothing from them enters the MIT game binary,
# which talks to this process through shared memory (port/runtime/gpu_ipc.h).
# Set FETCHCONTENT_SOURCE_DIR_PARALLEL_GS=/path/to/checkout to use a local tree.
cmake_minimum_required(VERSION 3.20)
project(xsg_gpu C CXX)

set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
set(XSG_PGS_COMMIT cc6184af7e0c03da603045ca371ffa5dae9b0655 CACHE STRING "paraLLEl-GS commit")
FetchContent_Declare(parallel_gs
  GIT_REPOSITORY https://github.com/Arntzen-Software/parallel-gs
  GIT_TAG ${XSG_PGS_COMMIT}
  GIT_SUBMODULES_RECURSE TRUE
  GIT_PROGRESS FALSE)
FetchContent_GetProperties(parallel_gs)
if(NOT parallel_gs_POPULATED)
  FetchContent_Populate(parallel_gs)
endif()

# Fix at the pinned commit: gif_transfer's batched-vertex path sizes its run from
# the whole buffer (size / nreg) instead of what is left after i, so a transfer
# that ends mid-packet runs past its end and the next transfer draws those
# vertices again.  The native renderer splits transfers at its effect snapshots;
# the battle help page's frame-feedback blend was drawn twice (washed-out,
# flickering frames).
set(_pgs_if ${parallel_gs_SOURCE_DIR}/gs/gs_interface.cpp)
file(READ ${_pgs_if} _pgs_src)
string(REPLACE "std::min<uint32_t>(size / nreg, path.tag.NLOOP - path.loop)"
               "std::min<uint32_t>((size - i) / nreg, path.tag.NLOOP - path.loop)" _pgs_fixed "${_pgs_src}")
# ...and with less than one whole loop left, take the per-register path (0 loops would not advance).
string(REPLACE "if (path.reg == 0 && optimized_draw_handler[path_index])"
               "if (path.reg == 0 && optimized_draw_handler[path_index] && size - i >= nreg)" _pgs_fixed "${_pgs_fixed}")
if(NOT _pgs_fixed STREQUAL _pgs_src)
  file(WRITE ${_pgs_if} "${_pgs_fixed}")
endif()

# Standalone mode (what upstream's PCSX2 integration uses): prebuilt shaders, no
# runtime shader compiler, no Granite application framework, GRANITE_PLATFORM=null.
# Window system integration is done here with SDL3.
set(PARALLEL_GS_STANDALONE ON CACHE BOOL "" FORCE)
add_subdirectory(${parallel_gs_SOURCE_DIR} ${parallel_gs_BINARY_DIR} EXCLUDE_FROM_ALL SYSTEM)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 IMPORTED_TARGET sdl3)

# The software GS's local-memory swizzle (MIT, ours) decodes VRAM for the stream.
set(XSG_GS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ps2-port-engine-gs)
# Native renderer shaders (native.vert/.frag, composite.comp) -> SPIR-V word lists included by native.cpp.
find_program(XSG_GLSLC glslc REQUIRED)
set(XSG_NATIVE_INC)
foreach(src native.vert native.frag composite.comp native_skin.vert native_mv.comp native_fx.comp crt.vert crt.frag shadow.comp)
  set(out ${CMAKE_CURRENT_BINARY_DIR}/${src}.inc)
  add_custom_command(OUTPUT ${out}
    COMMAND ${XSG_GLSLC} -O --target-env=vulkan1.1 -mfmt=num -o ${out} ${CMAKE_CURRENT_SOURCE_DIR}/${src}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src} VERBATIM)
  list(APPEND XSG_NATIVE_INC ${out})
endforeach()
# native.frag again with per-pixel character lighting (SKIN_PPL), for native_skin.cpp.
set(out ${CMAKE_CURRENT_BINARY_DIR}/native_ppl.frag.inc)
add_custom_command(OUTPUT ${out}
  COMMAND ${XSG_GLSLC} -O --target-env=vulkan1.1 -mfmt=num -DSKIN_PPL -fshader-stage=frag -o ${out} ${CMAKE_CURRENT_SOURCE_DIR}/native.frag
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/native.frag VERBATIM)
list(APPEND XSG_NATIVE_INC ${out})
# In-game menu (menu/, port/docs/MENU.md): Dear ImGui (MIT, vendored in
# port/third_party/imgui) with its SDL3 platform backend, drawn through Granite.
set(XSG_IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/imgui)
foreach(src imgui.vert imgui.frag)
  set(out ${CMAKE_CURRENT_BINARY_DIR}/${src}.inc)
  add_custom_command(OUTPUT ${out}
    COMMAND ${XSG_GLSLC} -O --target-env=vulkan1.1 -mfmt=num -o ${out} ${CMAKE_CURRENT_SOURCE_DIR}/menu/${src}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/menu/${src} VERBATIM)
  list(APPEND XSG_NATIVE_INC ${out})
endforeach()
# The menu font (Rajdhani, SIL OFL 1.1, menu/fonts/OFL.txt) embedded as byte lists.
# The rebuilt game UI's fonts (Exo 2, Saira Semi Condensed; SIL OFL 1.1, menu/fonts/OFL-game-ui.txt).
foreach(font Rajdhani-Medium.ttf Rajdhani-SemiBold.ttf Exo2-Medium.ttf Exo2-SemiBold.ttf Exo2-ExtraBold.ttf SairaSemiCondensed-ExtraBold.ttf SairaSemiCondensed-Medium.ttf)
  set(out ${CMAKE_CURRENT_BINARY_DIR}/${font}.inc)
  add_custom_command(OUTPUT ${out}
    COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_CURRENT_SOURCE_DIR}/menu/fonts/${font} -DOUT=${out}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/menu/embed.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/menu/fonts/${font} ${CMAKE_CURRENT_SOURCE_DIR}/menu/embed.cmake VERBATIM)
  list(APPEND XSG_NATIVE_INC ${out})
endforeach()
set(XSG_MENU_SRCS ui/dialog.cpp ui/style.cpp ui/art.cpp ui/menus.cpp ui/layout.cpp ui/prompts.cpp menu/menu.cpp menu/menu_render.cpp menu/option_rows.cpp menu/cfg.cpp
  ${XSG_IMGUI_DIR}/imgui.cpp ${XSG_IMGUI_DIR}/imgui_draw.cpp ${XSG_IMGUI_DIR}/imgui_tables.cpp
  ${XSG_IMGUI_DIR}/imgui_widgets.cpp)
find_package(Git QUIET)
set(XSG_BUILD_VERSION "unknown")
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --dirty --tags
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE XSG_BUILD_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
string(TIMESTAMP XSG_BUILD_DATE "%Y-%m-%d")
set_source_files_properties(menu/menu.cpp PROPERTIES COMPILE_DEFINITIONS
  "XSG_BUILD_VERSION=\"${XSG_BUILD_VERSION} (${XSG_BUILD_DATE})\"")
add_executable(xsg-gpu xsg_gpu.cpp xsg_input.cpp glyphs.cpp native.cpp native_skin.cpp native_fx.cpp native_prof.cpp ${XSG_MENU_SRCS} ${XSG_NATIVE_INC} ${XSG_GS_DIR}/src/gs_memory.cpp)
set_source_files_properties(${XSG_GS_DIR}/src/gs_memory.cpp PROPERTIES COMPILE_OPTIONS "-msse4.1;-w")
target_include_directories(xsg-gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../runtime ${XSG_GS_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR} ${parallel_gs_SOURCE_DIR}/Granite/third_party/stb/stb ${XSG_IMGUI_DIR} ${XSG_IMGUI_DIR}/backends)
target_link_libraries(xsg-gpu PRIVATE parallel-gs granite-vulkan pthread)
if(SDL3_FOUND)
  target_compile_definitions(xsg-gpu PRIVATE XSG_GPU_SDL=1)
  target_sources(xsg-gpu PRIVATE ${XSG_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp)
  target_link_libraries(xsg-gpu PRIVATE PkgConfig::SDL3)
else()
  message(WARNING "xsg-gpu: SDL3 not found (pkg-config sdl3); headless only")
endif()
target_compile_options(xsg-gpu PRIVATE -O2 -g -Wall -Wno-unused-parameter)
set_source_files_properties(${XSG_IMGUI_DIR}/imgui.cpp ${XSG_IMGUI_DIR}/imgui_draw.cpp ${XSG_IMGUI_DIR}/imgui_tables.cpp
  ${XSG_IMGUI_DIR}/imgui_widgets.cpp ${XSG_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp PROPERTIES COMPILE_OPTIONS "-w")

# Input prompt icons (port/assets/prompts, CC0): read from the source tree.
set_source_files_properties(ui/prompts.cpp PROPERTIES COMPILE_DEFINITIONS "XSG_PROMPTS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../assets/prompts\"")

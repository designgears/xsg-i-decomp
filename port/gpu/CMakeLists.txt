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

# Standalone mode (what upstream's PCSX2 integration uses): prebuilt shaders, no
# runtime shader compiler, no Granite application framework, GRANITE_PLATFORM=null.
# Window system integration is done here with SDL3.
set(PARALLEL_GS_STANDALONE ON CACHE BOOL "" FORCE)
add_subdirectory(${parallel_gs_SOURCE_DIR} ${parallel_gs_BINARY_DIR} EXCLUDE_FROM_ALL SYSTEM)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 IMPORTED_TARGET sdl3)

# The software GS's local-memory swizzle (MIT, ours) decodes VRAM for the stream.
set(XSG_GS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ps2-port-engine-gs)
add_executable(xsg-gpu xsg_gpu.cpp xsg_input.cpp ${XSG_GS_DIR}/src/gs_memory.cpp)
set_source_files_properties(${XSG_GS_DIR}/src/gs_memory.cpp PROPERTIES COMPILE_OPTIONS "-msse4.1;-w")
target_include_directories(xsg-gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../runtime ${XSG_GS_DIR}/include)
target_link_libraries(xsg-gpu PRIVATE parallel-gs granite-vulkan pthread)
if(SDL3_FOUND)
  target_compile_definitions(xsg-gpu PRIVATE XSG_GPU_SDL=1)
  target_link_libraries(xsg-gpu PRIVATE PkgConfig::SDL3)
else()
  message(WARNING "xsg-gpu: SDL3 not found (pkg-config sdl3); headless only")
endif()
target_compile_options(xsg-gpu PRIVATE -O2 -g -Wall -Wno-unused-parameter)

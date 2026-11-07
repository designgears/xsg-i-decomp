# VU1 C references (port/render/vu1_ref/) and their capture test.
# Included from port/CMakeLists.txt; not part of the default build.
#   cmake --build port/build-wt --target test_vu1_ref
#   port/build-wt/test_vu1_ref /tmp/vu1cap/vu1cap_*.bin
file(GLOB XSG_VU1REF_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/vu1_ref/*.c")
add_library(xsg_vu1_ref STATIC ${XSG_VU1REF_SRCS})
# Same float environment as vu1.cpp: SSE scalar math, no contraction.
target_compile_options(xsg_vu1_ref PRIVATE -m32 -std=gnu11 -O2 -g -msse2 -mfpmath=sse -ffp-contract=off -Wall)
set_target_properties(xsg_vu1_ref PROPERTIES EXCLUDE_FROM_ALL TRUE)

add_executable(test_vu1_ref EXCLUDE_FROM_ALL ${CMAKE_CURRENT_LIST_DIR}/../tests/test_vu1_ref.c)
target_compile_options(test_vu1_ref PRIVATE -m32 -std=gnu11 -O2 -g -msse2 -mfpmath=sse -ffp-contract=off -Wall -D_FILE_OFFSET_BITS=64)
target_link_options(test_vu1_ref PRIVATE -m32)
target_link_libraries(test_vu1_ref xsg_vu1_ref m)

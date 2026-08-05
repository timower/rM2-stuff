cmake_minimum_required(VERSION 3.10)
project(qsgepaper-preload CXX ASM)

# swtcon now also builds for host, but these tools dlopen the real armv7
# .so and read ARM-specific ucontext_t registers - still arm-only.
if(NOT TARGET swtcon OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
  return()
endif()

set(SWTCON_LIB_DIR ${CMAKE_SOURCE_DIR}/libs/swtcon)

# qsgepaper-test is the real thing: swtcon end-to-end, exactly as any real
# consumer would use it (init/lock/update/unlock_post/wait/shutdown), so it
# links the swtcon static library rather than compiling any of its sources
# itself.
add_executable(qsgepaper-test main.cpp)
target_link_libraries(qsgepaper-test PRIVATE swtcon)

# The probes/verifiers/benchmarks below deliberately do NOT link the swtcon
# library target. Each defines its own swtcon_runtime_offset()/
# swtcon_lib_impl_enabled() and dlopen's libqsgepaper.so independently,
# bypassing swtcon.cpp's production load_lib()/entry-point path entirely, so
# that swtcon.cpp's own definitions of those symbols are never in the link
# at all (linking the library here would risk pulling in swtcon.cpp's
# object file - and its conflicting definitions of those same symbols - the
# moment any of these probes references something else from it). They
# compile the specific internal translation units they need directly from
# libs/swtcon instead.
set_source_files_properties(${SWTCON_LIB_DIR}/playback_kernel_intrinsics.cpp
                             PROPERTIES COMPILE_OPTIONS "-O3")

# A/B verification tool for render_update_kernel (still-library, Phase 6) -
# see render_kernel_verify.cpp's header comment.
add_executable(render-kernel-verify render_kernel_verify.cpp ${SWTCON_LIB_DIR}/init.cpp)
target_link_libraries(render-kernel-verify dl)
target_include_directories(render-kernel-verify PRIVATE ${SWTCON_LIB_DIR} ${SWTCON_LIB_DIR}/include)

# Empirical input->output addressing mapper for render_update_kernel (Phase 6
# porting work) - see render_kernel_addr_map.cpp's header comment.
add_executable(render-kernel-addr-map render_kernel_addr_map.cpp ${SWTCON_LIB_DIR}/init.cpp)
target_link_libraries(render-kernel-addr-map dl)
target_include_directories(render-kernel-addr-map PRIVATE ${SWTCON_LIB_DIR} ${SWTCON_LIB_DIR}/include)

# Empirical probe for dispatch_processed_regions (still-library, the last
# remaining by-address call in the native port) - see
# dispatch_processed_regions_probe.cpp's header comment.
add_executable(dispatch-processed-regions-probe dispatch_processed_regions_probe.cpp
                                                 ${SWTCON_LIB_DIR}/init.cpp ${SWTCON_LIB_DIR}/update.cpp)
target_link_libraries(dispatch-processed-regions-probe dl)
target_include_directories(dispatch-processed-regions-probe PRIVATE ${SWTCON_LIB_DIR} ${SWTCON_LIB_DIR}/include)

# Empirical probe for the two worker-side playback kernels (still-library,
# the last remaining by-address calls in the pipeline) - see
# playback_kernel_probe.cpp's header comment.
add_executable(playback-kernel-probe playback_kernel_probe.cpp
                                     ${SWTCON_LIB_DIR}/init.cpp ${SWTCON_LIB_DIR}/update.cpp)
target_link_libraries(playback-kernel-probe dl)
target_include_directories(playback-kernel-probe PRIVATE ${SWTCON_LIB_DIR} ${SWTCON_LIB_DIR}/include)

# Microbenchmark for playback_kernel_plain_intrinsics (libs/swtcon/playback_kernel_intrinsics.cpp),
# the native port of the two worker-side playback kernels - measures its
# compute cost in isolation and, where the library is available, against the
# real NEON FUN_0004a140/FUN_0004a234 - see playback_kernel_bench.cpp's header
# comment. Links display.cpp (for the surrounding dispatch code) and
# playback_kernel_intrinsics.cpp (the kernel itself); deliberately does NOT link
# swtcon.cpp - it defines its own minimal swtcon_runtime_offset(), same as
# playback_kernel_probe.cpp, to dlopen the library independently rather than
# through swtcon.cpp's production load_lib() path.
add_executable(playback-kernel-bench playback_kernel_bench.cpp
                                      ${SWTCON_LIB_DIR}/init.cpp ${SWTCON_LIB_DIR}/update.cpp
                                      ${SWTCON_LIB_DIR}/display.cpp ${SWTCON_LIB_DIR}/playback_kernel_intrinsics.cpp
                                      ${SWTCON_LIB_DIR}/playback_kernel.s ${SWTCON_LIB_DIR}/playback_kernel_aligned.s)
target_link_libraries(playback-kernel-bench dl)
target_include_directories(playback-kernel-bench PRIVATE ${SWTCON_LIB_DIR} ${SWTCON_LIB_DIR}/include)

install(
  TARGETS qsgepaper-test
  COMPONENT tools
  DESTINATION ${OPT_PREFIX}bin)

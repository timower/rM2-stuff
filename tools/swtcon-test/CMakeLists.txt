cmake_minimum_required(VERSION 3.10)
project(swtcon-test CXX ASM)

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

# playback-kernel-bench below deliberately does NOT link the swtcon library
# target. It defines its own minimal swtcon_lib_impl_enabled() stub instead of
# going through swtcon.cpp's production load_lib()/entry-point path, so that
# swtcon.cpp's own definitions of those symbols are never in the link at all
# (linking the library here would risk pulling in swtcon.cpp's object file -
# and its conflicting definitions of those same symbols - the moment it
# references something else from it). It compiles the specific internal
# translation units it needs directly from libs/swtcon instead.
set_source_files_properties(${SWTCON_LIB_DIR}/playback_kernel_intrinsics.cpp
                             PROPERTIES COMPILE_OPTIONS "-O3")

# playback_kernel_intrinsics.cpp compiled twice more, forced into
# KERNEL_MODE_C/KERNEL_MODE_NEON under a distinct link symbol each (see its
# SWTCON_KERNEL_SYMBOL_SUFFIX comment) - production only ever builds ONE mode,
# picked by target arch (libs/swtcon/CMakeLists.txt). This is the only way to
# get every kernel implementation - scalar, NEON intrinsics, and (below) the
# raw ASM port - linked into one binary so playback-kernel-bench can compare
# them directly, with no libqsgepaper.so dlopen needed.
add_library(playback_kernel_c OBJECT ${SWTCON_LIB_DIR}/playback_kernel_intrinsics.cpp)
target_compile_definitions(playback_kernel_c PRIVATE SWTCON_KERNEL_MODE=1
                                                      SWTCON_KERNEL_SYMBOL_SUFFIX=_c)
target_include_directories(playback_kernel_c PRIVATE ${SWTCON_LIB_DIR} ${SWTCON_LIB_DIR}/include)
target_compile_features(playback_kernel_c PRIVATE cxx_std_20)

# -mfpu=neon: the rm-toolchain preset's compiler driver always passes this
# (see cmake/rm-toolchain.cmake's CMAKE_CXX_COMPILER_ARG1), but a plain
# nixpkgs armv7l cross toolchain targets a generic ARMv7 baseline without it
# - the real device (Cortex-A7) always has NEON, so force it just for this
# TU rather than relying on the ambient toolchain default.
add_library(playback_kernel_neon OBJECT ${SWTCON_LIB_DIR}/playback_kernel_intrinsics.cpp)
target_compile_definitions(playback_kernel_neon PRIVATE SWTCON_KERNEL_MODE=3
                                                         SWTCON_KERNEL_SYMBOL_SUFFIX=_neon)
target_compile_options(playback_kernel_neon PRIVATE -mfpu=neon)
target_include_directories(playback_kernel_neon PRIVATE ${SWTCON_LIB_DIR} ${SWTCON_LIB_DIR}/include)
target_compile_features(playback_kernel_neon PRIVATE cxx_std_20)

# Microbenchmark comparing every pure (no dlopen, no shared globals) playback
# kernel implementation this codebase has - see playback_kernel_bench.cpp's
# header comment. Links display.cpp (for the surrounding dispatch code),
# playback_kernel_intrinsics.cpp in its default (ASM-wrapping) mode for
# display.cpp's own needs, the raw playback_kernel.s/playback_kernel_aligned.s
# ASM port, and the two extra scalar/NEON object libraries above; deliberately
# does NOT link swtcon.cpp (see comment above).
add_executable(playback-kernel-bench playback_kernel_bench.cpp
                                      ${SWTCON_LIB_DIR}/init.cpp ${SWTCON_LIB_DIR}/wbf.cpp ${SWTCON_LIB_DIR}/update.cpp
                                      ${SWTCON_LIB_DIR}/display.cpp ${SWTCON_LIB_DIR}/playback_kernel_intrinsics.cpp
                                      ${SWTCON_LIB_DIR}/playback_kernel.s ${SWTCON_LIB_DIR}/playback_kernel_aligned.s
                                      $<TARGET_OBJECTS:playback_kernel_c> $<TARGET_OBJECTS:playback_kernel_neon>)
target_include_directories(playback-kernel-bench PRIVATE ${SWTCON_LIB_DIR} ${SWTCON_LIB_DIR}/include)
# Compiles update.h directly (see comment above) rather than linking the
# swtcon target, so it needs the same C++20 requirement (std::counting_semaphore
# in UpdateQueueGlobals) spelled out here too.
target_compile_features(playback-kernel-bench PRIVATE cxx_std_20)

install(
  TARGETS qsgepaper-test playback-kernel-bench
  COMPONENT tools
  DESTINATION ${OPT_PREFIX}bin)

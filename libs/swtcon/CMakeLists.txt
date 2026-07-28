# The playback kernels (playback_kernel.s/playback_kernel_aligned.s) are
# hand-transliterated ARMv7/NEON assembly targeting the reMarkable 2's
# 32-bit Cortex-A7 - they cannot assemble for any other target, so (like the
# implementation this replaced) this library only builds when targeting a
# 32-bit platform.
if(NOT "${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  return()
endif()

project(swtcon CXX ASM)

# The project's "release" preset ships MinSizeRel (-Os, optimize for code
# size) - a reasonable default for the binary as a whole, but a bad fit for
# playback_kernel_intrinsics.cpp specifically: it's a tight, compute-bound,
# per-pixel hot loop (see AGENTS.md's Phase 9) where -Os measurably leaves
# performance on the table (loop unrolling/scheduling -Os intentionally
# skips). Override just this one file to -O3 regardless of the overall
# build type - a later flag wins over the build type's -Os/-Og/etc, so this
# doesn't need to touch CMAKE_BUILD_TYPE or affect the rest of the binary's
# size.
set_source_files_properties(playback_kernel_intrinsics.cpp PROPERTIES COMPILE_OPTIONS "-O3")

add_library(${PROJECT_NAME} STATIC
  swtcon.cpp
  init.cpp
  update.cpp
  display.cpp
  playback_kernel_intrinsics.cpp
  playback_kernel.s
  playback_kernel_aligned.s)

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

# The public swtcon_* API lives under include/; everything else (init.h,
# update.h, display.h, qsgepaper_globals.h, statebuffer_table.h,
# swtcon_libimpl.h) is an internal header that the tools/ probes/benchmarks
# also reach into directly to drive and inspect individual pieces of the
# pipeline (the same "test tools deliberately reach past the public API"
# methodology described throughout AGENTS.md) - so both directories are
# PUBLIC include paths of this target rather than splitting a hard
# public/private boundary.
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# libqsgepaper.so was built against the C++11 std::string SSO ABI
# (_GLIBCXX_USE_CXX11_ABI=1): in qsgepaper_init the ModeEntry name string has a
# 0x18-byte (24) layout and FUN_0004535c reads ModeEntry::luts at offset 0x18.
# That only lines up if our std::string is also 24 bytes (new ABI). This is the
# modern default, but pin it so a toolchain default change can't silently break
# the shared ModeEntry/vector/list layouts. PUBLIC: every TU that links against
# this library (including the tools/ probes/benchmarks, which pass ModeEntry/
# LUTEntry/WorkItem across the boundary) must agree on the same ABI.
target_compile_definitions(${PROJECT_NAME} PUBLIC _GLIBCXX_USE_CXX11_ABI=1)

target_link_libraries(${PROJECT_NAME} PUBLIC pthread dl)

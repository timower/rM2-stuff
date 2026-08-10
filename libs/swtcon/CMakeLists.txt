project(swtcon CXX ASM)

# The ARMv7/NEON asm kernels only assemble for arm; other targets (e.g.
# dev-host) get playback_kernel_intrinsics.cpp's portable KERNEL_MODE_C.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
  set(SWTCON_KERNEL_MODE 2) # KERNEL_MODE_ASM
  set(SWTCON_ASM_SOURCES playback_kernel.s playback_kernel_aligned.s)
else()
  set(SWTCON_KERNEL_MODE 1) # KERNEL_MODE_C
  set(SWTCON_ASM_SOURCES)
endif()

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
  ${SWTCON_ASM_SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE SWTCON_KERNEL_MODE=${SWTCON_KERNEL_MODE})

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

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME} PUBLIC pthread dl)

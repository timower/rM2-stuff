# Prebuilt libghostty-vt (zig, not CMake) -- prefix supplied by
# nix/pkgs/ghostty-vt.nix via -DGHOSTTY_VT_PREFIX, or point it at a manual
# `zig build -Demit-lib-vt --prefix <dir>` output for local iteration.
if(GHOSTTY_VT_PREFIX)
  add_library(ghostty-vt STATIC IMPORTED GLOBAL)
  set_target_properties(
    ghostty-vt
    PROPERTIES IMPORTED_LOCATION "${GHOSTTY_VT_PREFIX}/lib/libghostty-vt.a"
               INTERFACE_INCLUDE_DIRECTORIES "${GHOSTTY_VT_PREFIX}/include")
  return()
endif()

# Outside nix: fetch ghostty and let its own CMakeLists.txt build
# libghostty-vt via `zig build -Demit-lib-vt`, which requires `zig` on PATH.
# Pin the same commit as nix/pkgs/ghostty-vt.nix so both paths agree.
find_program(GHOSTTY_VT_ZIG_EXECUTABLE zig REQUIRED)

# When cross-compiling for the rM2 (see cmake/rm-toolchain.cmake and the
# toltec toolchain), zig needs telling explicitly -- it can't infer a target
# from the C toolchain. Mirrors flake.nix's ghostty-vt-cross/zigTargetFlags.
set(_ghostty_vt_zig_flags "")
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
  list(APPEND _ghostty_vt_zig_flags "-Dtarget=arm-linux-gnueabihf"
       "-Dcpu=cortex_a7")
endif()
set(GHOSTTY_ZIG_BUILD_FLAGS "${_ghostty_vt_zig_flags}" CACHE STRING "" FORCE)

include(FetchContent)
FetchContent_Declare(
  ghostty
  GIT_REPOSITORY https://github.com/ghostty-org/ghostty.git
  GIT_TAG f64f4aca2c29b554d111b36c3d946a9bddd159ff)
FetchContent_MakeAvailable(ghostty)

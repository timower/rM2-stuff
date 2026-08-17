# Prebuilt libghostty-vt (zig, not CMake) -- prefix supplied by
# nix/pkgs/ghostty-vt.nix via -DGHOSTTY_VT_PREFIX, or point it at a manual
# `zig build -Demit-lib-vt --prefix <dir>` output for local iteration.
if(NOT GHOSTTY_VT_PREFIX)
  return()
endif()

add_library(ghostty-vt STATIC IMPORTED GLOBAL)
set_target_properties(
  ghostty-vt
  PROPERTIES IMPORTED_LOCATION "${GHOSTTY_VT_PREFIX}/lib/libghostty-vt.a"
             INTERFACE_INCLUDE_DIRECTORIES "${GHOSTTY_VT_PREFIX}/include")

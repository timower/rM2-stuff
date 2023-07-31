add_library(frida-gum STATIC IMPORTED GLOBAL)
add_library(Frida::Gum ALIAS frida-gum)
set_target_properties(frida-gum PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libfrida-gum.a"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

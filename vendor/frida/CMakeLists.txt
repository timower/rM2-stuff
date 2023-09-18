add_library(frida-gum STATIC IMPORTED GLOBAL)
add_library(Frida::Gum ALIAS frida-gum)

set(SYSTEM_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

set_target_properties(frida-gum PROPERTIES
  IMPORTED_LOCATION "${SYSTEM_DIR}/libfrida-gum.a"
  INTERFACE_INCLUDE_DIRECTORIES "${SYSTEM_DIR}")

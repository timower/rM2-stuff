include(FetchContent)

set(FRIDA_VERSION "16.1.4")

# Platform
set(PLATFORM "linux")
if (APPLE)
  set(PLATFORM "macos")
endif()

# Architecture
set(ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
if("${ARCHITECTURE}" STREQUAL "arm")
  set(ARCHITECTURE "armhf")
endif()

set(URL_BASE "https://github.com/frida/frida/releases/download/${FRIDA_VERSION}")
set(URL
  "${URL_BASE}/frida-gum-devkit-${FRIDA_VERSION}-${PLATFORM}-${ARCHITECTURE}.tar.xz")

message(STATUS "Getting frida from ${URL}")
FetchContent_Declare(frida-gum
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  URL "${URL}")

FetchContent_MakeAvailable(frida-gum)

add_library(frida-gum STATIC IMPORTED GLOBAL)
add_library(Frida::Gum ALIAS frida-gum)

set_target_properties(frida-gum PROPERTIES
  IMPORTED_LOCATION "${frida-gum_SOURCE_DIR}/libfrida-gum.a"
  INTERFACE_INCLUDE_DIRECTORIES "${frida-gum_SOURCE_DIR}")

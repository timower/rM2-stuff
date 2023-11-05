include(FetchContent)

add_subdirectory(frida)
add_subdirectory(linux)
add_subdirectory(tomlplusplus)
add_subdirectory(noto)
add_subdirectory(expected)
add_subdirectory(utfcpp)

add_subdirectory(libYaft)
add_subdirectory(libTilEm)

if (${BUILD_TESTS})
  add_subdirectory(catch2)
endif()

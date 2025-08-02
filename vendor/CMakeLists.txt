include(FetchContent)

add_subdirectory(frida)
add_subdirectory(linux)
add_subdirectory(tomlplusplus)
add_subdirectory(noto)
add_subdirectory(expected)
add_subdirectory(utfcpp)

add_subdirectory(libYaft)
add_subdirectory(libTilEm)

add_subdirectory(thick)
add_subdirectory(stb)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  add_subdirectory(evdev)
endif()

if(${BUILD_TESTS})
  add_subdirectory(catch2)
endif()

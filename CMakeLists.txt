cmake_minimum_required(VERSION 3.19)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

project(rm2-stuff VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-psabi -Wno-gnu-statement-expression")

add_compile_options(-fdiagnostics-color=always)

add_subdirectory(libs)
add_subdirectory(tools)
add_subdirectory(apps)


set(CPACK_GENERATOR "DEB")
set(CPACK_POST_BUILD_SCRIPTS "${PROJECT_SOURCE_DIR}/cmake/GenIPK.cmake")
configure_file("${PROJECT_SOURCE_DIR}/cmake/CPackOptions.cmake.in"
               "${PROJECT_BINARY_DIR}/CPackOptions.cmake"
               @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/CPackOptions.cmake")

include(CPack)

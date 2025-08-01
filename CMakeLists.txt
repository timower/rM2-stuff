cmake_minimum_required(VERSION 3.19)

project(
  rm2-stuff
  VERSION 0.1.3
  LANGUAGES CXX C)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(BuildTypes)
include(Deploy)

set(CMAKE_CXX_STANDARD "17")

add_subdirectory(libs)
add_subdirectory(tools)
add_subdirectory(apps)

add_subdirectory(vendor EXCLUDE_FROM_ALL)

option(BUILD_TESTS "Build Tests" OFF)
if(${BUILD_TESTS})
  include(CTest)
  enable_testing()
  add_subdirectory(test)
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_POST_BUILD_SCRIPTS "${PROJECT_SOURCE_DIR}/cmake/GenIPK.cmake")

get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")

configure_file("${PROJECT_SOURCE_DIR}/cmake/CPackOptions.cmake.in"
               "${PROJECT_BINARY_DIR}/CPackOptions.cmake" @ONLY)

set(CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/CPackOptions.cmake")

include(CPack)

add_opkg_targets()

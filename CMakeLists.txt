cmake_minimum_required(VERSION 3.19)

# Prevent in source builds
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project(
  rm2-stuff
  VERSION 0.1.3
  LANGUAGES CXX C)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(BuildTypes)
include(Deploy)
include(Packaging)

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

setup_cpack_ipk()
add_opkg_targets()

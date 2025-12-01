project(unit-tests)

add_executable(${PROJECT_NAME} TestUnistdpp.cpp TestRMLib.cpp TestTilem.cpp
                               TestYaft.cpp TestRocket.cpp TestRm2fb.cpp)

find_program(CAT_EXE cat)

target_compile_definitions(
  ${PROJECT_NAME} PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets"
                          CAT_EXE="${CAT_EXE}")

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE Catch2::Catch2WithMain
          unistdpp
          rMlib
          tilem::lib
          Yaft::app_lib
          rocket::lib
          rm2fb_headers)

# From:
# https://github.com/catchorg/Catch2/blob/devel/docs/cmake-integration.md#catchcmake-and-catchaddtestscmake
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

catch_discover_tests(${PROJECT_NAME} EXTRA_ARGS "--allow-running-no-tests")

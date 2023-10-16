project(unit-tests)

include(FetchContent)
FetchContent_Declare(
  catch2
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz
)
FetchContent_MakeAvailable(catch2)

add_executable(${PROJECT_NAME}
  TestUnistdpp.cpp
  TestRMLib.cpp
  TestTilem.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    Catch2::Catch2WithMain
    unistdpp
    rMlib
    tilem::lib)

add_test(NAME "unit" COMMAND ${PROJECT_NAME})

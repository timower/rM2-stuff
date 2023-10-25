project(unit-tests)

add_executable(${PROJECT_NAME}
  TestUnistdpp.cpp
  TestRMLib.cpp
  TestTilem.cpp
  TestYaft.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    Catch2::Catch2WithMain
    unistdpp
    rMlib
    tilem::lib
    Yaft::app_lib)

add_test(NAME "unit" COMMAND ${PROJECT_NAME})

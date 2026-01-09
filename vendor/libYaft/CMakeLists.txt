project(libYaft)

add_library(${PROJECT_NAME} STATIC terminal.cpp parse.c)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND tic -o "${CMAKE_BINARY_DIR}/terminfo"
          "${CMAKE_CURRENT_SOURCE_DIR}/info/yaft.src")

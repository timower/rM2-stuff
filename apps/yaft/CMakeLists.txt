project(yaft)

add_executable(${PROJECT_NAME}
  main.cpp
  terminal.cpp
  parse.c
  fb/common.cpp
  keyboard.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rMlib)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND tic ${CMAKE_CURRENT_SOURCE_DIR}/info/yaft.src -o ${CMAKE_BINARY_DIR}/terminfo)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES draft/yaft.draft DESTINATION etc/draft)
install(FILES draft/yaft.png DESTINATION etc/draft/icons)
install(DIRECTORY ${CMAKE_BINARY_DIR}/terminfo DESTINATION share)

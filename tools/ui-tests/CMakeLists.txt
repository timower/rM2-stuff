project(ui-tests)

add_executable(${PROJECT_NAME}
  main.cpp)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rMlib)

add_deploy(${PROJECT_NAME})

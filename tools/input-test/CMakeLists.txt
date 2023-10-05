project(input-test)

if (EMULATE AND NOT EMULATE_UINPUT)
  return()
endif()

add_executable(${PROJECT_NAME}
  main.cpp)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rMlib)

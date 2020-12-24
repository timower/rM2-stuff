project(wabbitemu)

add_subdirectory(lib)

add_executable(${PROJECT_NAME}
  main.cpp
  ${WABBIT_SRC})

target_compile_definitions(${PROJECT_NAME} PRIVATE _LINUX)

target_include_directories(${PROJECT_NAME} PRIVATE ${WABBIT_INCLUDE})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    tiemu)

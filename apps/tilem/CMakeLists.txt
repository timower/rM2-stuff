project(tilem)

add_subdirectory(emu)

add_executable(${PROJECT_NAME}
  main.cpp
  ${WABBIT_SRC})

target_compile_definitions(${PROJECT_NAME} PRIVATE _LINUX)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_include_directories(${PROJECT_NAME} PRIVATE ${WABBIT_INCLUDE})

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    tiemu
    rMlib)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES draft/tilem.draft DESTINATION etc/draft)
install(FILES draft/tilem.png DESTINATION etc/draft/icons)

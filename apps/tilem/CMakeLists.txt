project(tilem)

add_subdirectory(emu)

add_library(tilem_lib OBJECT
   KeyPad.cpp Screen.cpp Dialog.cpp Calculator.cpp)
add_library(tilem::lib ALIAS tilem_lib)

target_link_libraries(tilem_lib PUBLIC rMlib tiemu)
target_include_directories(tilem_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(${PROJECT_NAME} main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE _LINUX)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    tilem::lib
    rMlib)

install(TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/bin)
install(FILES draft/tilem.draft
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/etc/draft)
install(FILES draft/tilem.png
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/etc/draft/icons)

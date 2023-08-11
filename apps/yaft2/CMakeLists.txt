project(yaft2)

add_executable(${PROJECT_NAME}
  main.cpp
  keyboard.cpp
  screen.cpp
  layout.cpp
  keymap.cpp
  config.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rMlib
    libYaft
    util
    tomlplusplus::tomlplusplus)

install(TARGETS ${PROJECT_NAME} DESTINATION opt/bin)

install(FILES draft/yaft2.draft DESTINATION opt/etc/draft)

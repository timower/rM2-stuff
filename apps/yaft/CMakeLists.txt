project(yaft)

add_executable(${PROJECT_NAME}
  main.cpp
  fb/common.cpp
  keyboard.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rMlib
    libYaft)


install(TARGETS ${PROJECT_NAME} DESTINATION opt/bin)

install(FILES draft/yaft.draft DESTINATION opt/etc/draft)
install(FILES draft/yaft.png DESTINATION opt/etc/draft/icons)

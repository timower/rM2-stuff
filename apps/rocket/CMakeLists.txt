project(rocket)

add_executable(${PROJECT_NAME}
  main.cpp
  App.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rMlib)

install(TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/bin/)
install(FILES rocket.service
  COMPONENT ${PROJECT_NAME}
  DESTINATION lib/systemd/system)

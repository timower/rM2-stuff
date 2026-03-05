project(keyhotd)

add_executable(${PROJECT_NAME} main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} PRIVATE rMlib)

install(
  TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  DESTINATION ${OPT_PREFIX}bin/)

install(
  FILES keyhotd.service
  COMPONENT ${PROJECT_NAME}
  DESTINATION lib/systemd/system)

project(rocket)

add_library(rocket_lib OBJECT App.cpp AppWidgets.cpp Launcher.cpp)
add_library(rocket::lib ALIAS rocket_lib)

target_link_libraries(rocket_lib PUBLIC rMlib)
target_include_directories(rocket_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(${PROJECT_NAME} main.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME} PRIVATE rocket_lib rMlib)

install(
  TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/bin/)
install(
  FILES rocket.service
  COMPONENT ${PROJECT_NAME}
  DESTINATION lib/systemd/system)

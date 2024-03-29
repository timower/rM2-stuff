project(yaft)

add_library(yaft_app_lib OBJECT
  YaftWidget.cpp keyboard.cpp screen.cpp layout.cpp keymap.cpp config.cpp)
add_library(Yaft::app_lib ALIAS yaft_app_lib)

target_include_directories(yaft_app_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(yaft_app_lib PUBLIC
    rMlib
    libYaft
    util
    tomlplusplus::tomlplusplus)

if (APPLE)
  target_link_libraries(yaft_app_lib PUBLIC linux::mxcfb)
endif()

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    yaft_app_lib
    rMlib)

install(TARGETS ${PROJECT_NAME}
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/bin)

install(DIRECTORY ${CMAKE_BINARY_DIR}/terminfo
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/share)

install(FILES draft/yaft.draft
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/etc/draft)
install(FILES draft/yaft.png
  COMPONENT ${PROJECT_NAME}
  DESTINATION opt/etc/draft/icons)

add_deploy(${PROJECT_NAME})
